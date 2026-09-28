#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <termios.h>
#include <limits.h>
#include <ctype.h>
#include <dirent.h>
#include <time.h>

#define SHELL_NAME "hasini_2520030102"
#define SHELL_MAX_INPUT 4096
#define MAX_TOKENS 512
#define MAX_ARGS 256
#define MAX_HISTORY 100
#define MAX_JOBS 100
#define MAX_PIPE 32

typedef struct {
    char *text;
} Token;

typedef struct {
    int id;
    pid_t pgid;
    char command[SHELL_MAX_INPUT];
    int running;
    int stopped;
} Job;

typedef struct {
    char **argv;
    char *input_file;
    char *output_file;
    char *append_file;
    char *error_file;
    int merge_error;
} Command;

static struct termios shell_tmodes;
static pid_t shell_pgid;
static int shell_terminal = STDIN_FILENO;
static int shell_interactive = 0;

static char *history[MAX_HISTORY];
static int history_count = 0;

static Job jobs[MAX_JOBS];
static int job_count = 0;

static volatile sig_atomic_t child_changed = 0;
static int last_status = 0;

/* ---------------------------------------------------------- */
/* SIGNAL HANDLING                                             */
/* ---------------------------------------------------------- */

static void sigchld_handler(int sig)
{
    (void)sig;
    child_changed = 1;
}

static void setup_shell(void)
{
    shell_interactive = isatty(shell_terminal);

    if (!shell_interactive)
        return;

    while (tcgetpgrp(shell_terminal) !=
           (shell_pgid = getpgrp()))
        kill(-shell_pgid, SIGTTIN);

    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    shell_pgid = getpid();

    if (setpgid(shell_pgid, shell_pgid) < 0 &&
        errno != EPERM)
        perror("setpgid");

    if (tcsetpgrp(shell_terminal, shell_pgid) < 0)
        perror("tcsetpgrp");

    if (tcgetattr(shell_terminal, &shell_tmodes) < 0)
        perror("tcgetattr");

    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));

    sigemptyset(&sa.sa_mask);

    sa.sa_handler = sigchld_handler;
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;

    sigaction(SIGCHLD, &sa, NULL);
}

/* ---------------------------------------------------------- */
/* HISTORY                                                      */
/* ---------------------------------------------------------- */

static void add_history(const char *line)
{
    if (!line || !*line)
        return;

    if (history_count > 0 &&
        strcmp(history[history_count - 1], line) == 0)
        return;

    if (history_count == MAX_HISTORY) {

        free(history[0]);

        memmove(
            history,
            history + 1,
            sizeof(history[0]) * (MAX_HISTORY - 1)
        );

        history_count--;
    }

    history[history_count++] = strdup(line);
}

static void show_history(void)
{
    for (int i = 0; i < history_count; i++)
        printf("%4d  %s\n", i + 1, history[i]);

    last_status = 0;
}

/* ---------------------------------------------------------- */
/* INPUT                                                       */
/* ---------------------------------------------------------- */

static void redraw_line(const char *prompt,
                        const char *buffer)
{
    printf("\r\033[2K%s%s", prompt, buffer);
    fflush(stdout);
}

static void enable_raw_mode(struct termios *old)
{
    if (tcgetattr(STDIN_FILENO, old) < 0)
        return;

    struct termios raw = *old;

    raw.c_lflag &= ~(ICANON | ECHO | IEXTEN);
    raw.c_iflag &= ~(IXON | ICRNL);

    raw.c_oflag |= OPOST;

    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    tcsetattr(
        STDIN_FILENO,
        TCSAFLUSH,
        &raw
    );
}

static void restore_terminal(struct termios *old)
{
    tcsetattr(
        STDIN_FILENO,
        TCSAFLUSH,
        old
    );
}

static int read_line(const char *prompt,
                     char *buffer,
                     size_t size)
{
    if (!shell_interactive) {

        if (!fgets(buffer, size, stdin))
            return 0;

        buffer[strcspn(buffer, "\n")] = '\0';

        return 1;
    }

    struct termios old;

    enable_raw_mode(&old);

    size_t len = 0;
    int history_pos = history_count;

    buffer[0] = '\0';

    printf("%s", prompt);
    fflush(stdout);

    while (1) {

        char c;

        ssize_t n = read(
            STDIN_FILENO,
            &c,
            1
        );

        if (n <= 0) {

            restore_terminal(&old);
            printf("\n");

            return 0;
        }

        if (c == '\n' || c == '\r') {

            buffer[len] = '\0';

            printf("\n");

            restore_terminal(&old);

            return 1;
        }

        if (c == 4) {

            if (len == 0) {

                restore_terminal(&old);
                printf("\n");

                return 0;
            }

            continue;
        }

        if (c == 127 || c == 8) {

            if (len > 0) {

                len--;

                buffer[len] = '\0';

                printf("\b \b");

                fflush(stdout);
            }

            continue;
        }

        if (c == 27) {

            char seq[2];

            if (read(STDIN_FILENO, &seq[0], 1) <= 0)
                continue;

            if (read(STDIN_FILENO, &seq[1], 1) <= 0)
                continue;

            if (seq[0] == '[' && seq[1] == 'A') {

                if (history_count > 0 &&
                    history_pos > 0) {

                    history_pos--;

                    strncpy(
                        buffer,
                        history[history_pos],
                        size - 1
                    );

                    buffer[size - 1] = '\0';

                    len = strlen(buffer);

                    redraw_line(prompt, buffer);
                }
            }

            else if (seq[0] == '[' &&
                     seq[1] == 'B') {

                if (history_pos <
                    history_count - 1) {

                    history_pos++;

                    strncpy(
                        buffer,
                        history[history_pos],
                        size - 1
                    );

                    buffer[size - 1] = '\0';

                    len = strlen(buffer);

                    redraw_line(prompt, buffer);
                }

                else if (history_pos ==
                         history_count - 1) {

                    history_pos = history_count;

                    len = 0;

                    buffer[0] = '\0';

                    redraw_line(prompt, buffer);
                }
            }

            continue;
        }

        if (isprint((unsigned char)c) &&
            len < size - 1) {

            buffer[len++] = c;

            buffer[len] = '\0';

            putchar(c);

            fflush(stdout);
        }
    }
}

/* ---------------------------------------------------------- */
/* VARIABLE EXPANSION                                          */
/* ---------------------------------------------------------- */

static char *expand_variables(const char *input)
{
    char *result = malloc(SHELL_MAX_INPUT);

    if (!result)
        exit(EXIT_FAILURE);

    size_t i = 0;
    size_t j = 0;

    while (input[i] &&
           j < SHELL_MAX_INPUT - 1) {

        if (input[i] != '$') {

            result[j++] = input[i++];

            continue;
        }

        if (input[i + 1] == '?') {

            char value[32];

            snprintf(
                value,
                sizeof(value),
                "%d",
                last_status
            );

            for (int k = 0;
                 value[k] &&
                 j < SHELL_MAX_INPUT - 1;
                 k++)
                result[j++] = value[k];

            i += 2;

            continue;
        }

        size_t start = i + 1;
        size_t end = start;

        char name[256];

        if (input[start] == '{') {

            start++;
            end = start;

            while (input[end] &&
                   input[end] != '}' &&
                   end - start <
                   sizeof(name) - 1)
                end++;

            if (input[end] == '}') {

                size_t n = end - start;

                memcpy(name,
                       input + start,
                       n);

                name[n] = '\0';

                const char *value =
                    getenv(name);

                if (!value)
                    value = "";

                for (size_t k = 0;
                     value[k] &&
                     j < SHELL_MAX_INPUT - 1;
                     k++)
                    result[j++] = value[k];

                i = end + 1;

                continue;
            }
        }

        if (isalpha((unsigned char)input[start]) ||
            input[start] == '_') {

            end = start;

            while ((isalnum(
                        (unsigned char)
                        input[end]) ||
                    input[end] == '_') &&
                   end - start <
                   sizeof(name) - 1)
                end++;

            size_t n = end - start;

            memcpy(
                name,
                input + start,
                n
            );

            name[n] = '\0';

            const char *value =
                getenv(name);

            if (!value)
                value = "";

            for (size_t k = 0;
                 value[k] &&
                 j < SHELL_MAX_INPUT - 1;
                 k++)
                result[j++] = value[k];

            i = end;

            continue;
        }

        result[j++] = input[i++];
    }

    result[j] = '\0';

    return result;
}

/* ---------------------------------------------------------- */
/* TOKENIZER                                                    */
/* ---------------------------------------------------------- */

static int tokenize(const char *input,
                    Token *tokens)
{
    int count = 0;
    size_t i = 0;

    while (input[i]) {

        while (isspace(
                   (unsigned char)input[i]))
            i++;

        if (!input[i])
            break;

        if (count >= MAX_TOKENS - 1)
            return -1;

        char buffer[SHELL_MAX_INPUT];

        size_t j = 0;

        int single = 0;
        int double_q = 0;

        while (input[i]) {

            char c = input[i];

            if (!single &&
                !double_q &&
                isspace((unsigned char)c))
                break;

            if (!single &&
                c == '\\') {

                i++;

                if (!input[i])
                    break;

                if (j < SHELL_MAX_INPUT - 1)
                    buffer[j++] = input[i++];

                continue;
            }

            if (!double_q &&
                c == '\'') {

                single = !single;
                i++;

                continue;
            }

            if (!single &&
                c == '"') {

                double_q = !double_q;
                i++;

                continue;
            }

            if (!single &&
                !double_q) {

                if (input[i] == '|' ||
                    input[i] == '<' ||
                    input[i] == '>' ||
                    input[i] == '&') {

                    if (j == 0) {

                        if (input[i] == '>' &&
                            input[i + 1] == '>') {

                            buffer[j++] = '>';
                            buffer[j++] = '>';
                            i += 2;
                        }

                        else if (input[i] == '2' &&
                                 input[i + 1] == '>') {

                            buffer[j++] = '2';
                            buffer[j++] = '>';
                            i += 2;
                        }

                        else if (input[i] == '>' &&
                                 input[i + 1] == '&' &&
                                 input[i + 2] == '1') {

                            buffer[j++] = '>';
                            buffer[j++] = '&';
                            buffer[j++] = '1';
                            i += 3;
                        }

                        else {

                            buffer[j++] = input[i++];
                        }

                        break;
                    }

                    break;
                }

                if (input[i] == '2' &&
                    input[i + 1] == '>' &&
                    j == 0) {

                    buffer[j++] = '2';
                    buffer[j++] = '>';
                    i += 2;

                    break;
                }
            }

            if (j < SHELL_MAX_INPUT - 1)
                buffer[j++] = input[i++];

            else
                return -1;
        }

        if (single || double_q) {

            fprintf(
                stderr,
                "syntax error: unmatched quote\n"
            );

            for (int k = 0; k < count; k++)
                free(tokens[k].text);

            return -1;
        }

        buffer[j] = '\0';

        if (j > 0) {

            tokens[count].text =
                expand_variables(buffer);

            count++;
        }

        while (isspace(
                   (unsigned char)input[i]))
            i++;
    }

    tokens[count].text = NULL;

    return count;
}

static void free_tokens(Token *tokens,
                         int count)
{
    for (int i = 0; i < count; i++)
        free(tokens[i].text);
}

/* ---------------------------------------------------------- */
/* COMMAND PARSER                                               */
/* ---------------------------------------------------------- */

static void initialize_commands(Command *commands)
{
    for (int i = 0; i < MAX_PIPE; i++) {

        memset(
            &commands[i],
            0,
            sizeof(Command)
        );

        commands[i].argv =
            calloc(
                MAX_ARGS,
                sizeof(char *)
            );
    }
}

static void free_commands(Command *commands)
{
    for (int i = 0; i < MAX_PIPE; i++) {

        if (commands[i].argv) {

            for (int j = 0;
                 commands[i].argv[j];
                 j++)
                free(commands[i].argv[j]);

            free(commands[i].argv);
        }

        free(commands[i].input_file);
        free(commands[i].output_file);
        free(commands[i].append_file);
        free(commands[i].error_file);
    }
}

static int parse_commands(
    Token *tokens,
    int count,
    Command *commands,
    int *command_count,
    int *background)
{
    int current = 0;
    int argc = 0;

    *background = 0;

    for (int i = 0; i < count; i++) {

        char *t = tokens[i].text;

        if (!strcmp(t, "|")) {

            if (argc == 0) {

                fprintf(
                    stderr,
                    "syntax error near '|'\n"
                );

                return -1;
            }

            commands[current].argv[argc] =
                NULL;

            current++;

            if (current >= MAX_PIPE) {

                fprintf(
                    stderr,
                    "too many pipeline commands\n"
                );

                return -1;
            }

            argc = 0;

            continue;
        }

        if (!strcmp(t, "<") ||
            !strcmp(t, ">") ||
            !strcmp(t, ">>") ||
            !strcmp(t, "2>")) {

            if (i + 1 >= count) {

                fprintf(
                    stderr,
                    "missing redirection file\n"
                );

                return -1;
            }

            char **target;

            if (!strcmp(t, "<"))
                target =
                    &commands[current].input_file;

            else if (!strcmp(t, ">"))
                target =
                    &commands[current].output_file;

            else if (!strcmp(t, ">>"))
                target =
                    &commands[current].append_file;

            else
                target =
                    &commands[current].error_file;

            free(*target);

            *target =
                strdup(tokens[++i].text);

            continue;
        }

        if (!strcmp(t, ">1") ||
            !strcmp(t, ">&1") ||
            !strcmp(t, "2>&1")) {

            commands[current].merge_error = 1;

            continue;
        }

        if (!strcmp(t, "&")) {

            if (i != count - 1) {

                fprintf(
                    stderr,
                    "syntax error: & must be last\n"
                );

                return -1;
            }

            *background = 1;

            continue;
        }

        if (argc >= MAX_ARGS - 1) {

            fprintf(
                stderr,
                "too many arguments\n"
            );

            return -1;
        }

        commands[current].argv[argc++] =
            strdup(t);
    }

    if (argc == 0) {

        fprintf(
            stderr,
            "empty command\n"
        );

        return -1;
    }

    commands[current].argv[argc] =
        NULL;

    *command_count = current + 1;

    return 0;
}

/* ---------------------------------------------------------- */
/* CALENDAR                                                      */
/* ---------------------------------------------------------- */

static int is_leap_year(int year)
{
    return (year % 400 == 0) ||
           (year % 4 == 0 &&
            year % 100 != 0);
}

static int days_in_month(int month,
                         int year)
{
    int days[] = {
        31, 28, 31, 30,
        31, 30, 31, 31,
        30, 31, 30, 31
    };

    if (month == 2 &&
        is_leap_year(year))
        return 29;

    return days[month - 1];
}

static void builtin_cal(char **args)
{
    time_t now = time(NULL);

    struct tm tm_data;

    localtime_r(
        &now,
        &tm_data
    );

    int month = tm_data.tm_mon + 1;
    int year = tm_data.tm_year + 1900;

    if (args[1]) {

        int m;
        int y;

        if (sscanf(args[1],
                   "%d",
                   &m) == 1) {

            month = m;

            if (args[2] &&
                sscanf(args[2],
                       "%d",
                       &y) == 1)
                year = y;
        }

        else {

            fprintf(
                stderr,
                "cal: usage: cal [month] [year]\n"
            );

            last_status = 1;

            return;
        }
    }

    if (month < 1 ||
        month > 12) {

        fprintf(
            stderr,
            "cal: invalid month\n"
        );

        last_status = 1;

        return;
    }

    struct tm first = {0};

    first.tm_year = year - 1900;
    first.tm_mon = month - 1;
    first.tm_mday = 1;

    mktime(&first);

    const char *months[] = {
        "January",
        "February",
        "March",
        "April",
        "May",
        "June",
        "July",
        "August",
        "September",
        "October",
        "November",
        "December"
    };

    printf(
        "      %s %d\n",
        months[month - 1],
        year
    );

    printf(
        "Su Mo Tu We Th Fr Sa\n"
    );

    int start = first.tm_wday;
    int days = days_in_month(
        month,
        year
    );

    for (int i = 0; i < start; i++)
        printf("   ");

    for (int day = 1;
         day <= days;
         day++) {

        printf("%2d", day);

        if ((start + day) % 7 == 0)
            printf("\n");
        else
            printf(" ");
    }

    if ((start + days) % 7 != 0)
        printf("\n");

    last_status = 0;
}

/* ---------------------------------------------------------- */
/* BUILT-IN COMMANDS                                            */
/* ---------------------------------------------------------- */

static void builtin_pwd(void)
{
    char cwd[PATH_MAX];

    if (getcwd(cwd, sizeof(cwd)))
        printf("%s\n", cwd);

    else {

        perror("pwd");
        last_status = 1;

        return;
    }

    last_status = 0;
}

static void builtin_cd(char **args)
{
    const char *path = args[1];

    if (!path) {

        path = getenv("HOME");

        if (!path) {

            fprintf(
                stderr,
                "cd: HOME not set\n"
            );

            last_status = 1;

            return;
        }
    }

    if (!strcmp(path, "-")) {

        path = getenv("OLDPWD");

        if (!path) {

            fprintf(
                stderr,
                "cd: OLDPWD not set\n"
            );

            last_status = 1;

            return;
        }

        printf("%s\n", path);
    }

    char old[PATH_MAX];

    if (!getcwd(old, sizeof(old))) {

        perror("cd");

        last_status = 1;

        return;
    }

    if (chdir(path) < 0) {

        perror("cd");

        last_status = 1;

        return;
    }

    setenv(
        "OLDPWD",
        old,
        1
    );

    char current[PATH_MAX];

    if (getcwd(current,
               sizeof(current)))
        setenv(
            "PWD",
            current,
            1
        );

    last_status = 0;
}

static void builtin_echo(char **args)
{
    int start = 1;
    int newline = 1;

    if (args[1] &&
        !strcmp(args[1], "-n")) {

        newline = 0;
        start = 2;
    }

    for (int i = start;
         args[i];
         i++) {

        if (i > start)
            putchar(' ');

        printf("%s", args[i]);
    }

    if (newline)
        putchar('\n');

    last_status = 0;
}

static void builtin_mkdir(char **args)
{
    if (!args[1]) {

        fprintf(
            stderr,
            "mkdir: missing operand\n"
        );

        last_status = 1;

        return;
    }

    int status = 0;

    for (int i = 1;
         args[i];
         i++) {

        if (mkdir(args[i], 0755) < 0) {

            perror(args[i]);

            status = 1;
        }
    }

    last_status = status;
}

static void builtin_touch(char **args)
{
    if (!args[1]) {

        fprintf(
            stderr,
            "touch: missing file\n"
        );

        last_status = 1;

        return;
    }

    int status = 0;

    for (int i = 1;
         args[i];
         i++) {

        int fd = open(
            args[i],
            O_WRONLY |
            O_CREAT,
            0644
        );

        if (fd < 0) {

            perror(args[i]);

            status = 1;
        }

        else
            close(fd);
    }

    last_status = status;
}

static void builtin_clear(void)
{
    printf("\033[2J\033[H");
    fflush(stdout);

    last_status = 0;
}

static void builtin_whoami(void)
{
    const char *user =
        getenv("USER");

    if (!user)
        user = getenv("USERNAME");

    if (user)
        printf("%s\n", user);
    else
        printf("unknown\n");

    last_status = 0;
}

static void builtin_uname(void)
{
    printf("hasini_2520030102 Linux Shell\n");
    last_status = 0;
}

static void builtin_export(char **args)
{
    if (!args[1]) {

        extern char **environ;

        for (char **e = environ;
             *e;
             e++)
            puts(*e);

        last_status = 0;

        return;
    }

    int status = 0;

    for (int i = 1;
         args[i];
         i++) {

        char *eq =
            strchr(args[i], '=');

        if (!eq) {

            fprintf(
                stderr,
                "export: use NAME=value\n"
            );

            status = 1;

            continue;
        }

        *eq = '\0';

        if (setenv(
                args[i],
                eq + 1,
                1) < 0) {

            perror("export");

            status = 1;
        }

        *eq = '=';
    }

    last_status = status;
}

static void builtin_unset(char **args)
{
    for (int i = 1;
         args[i];
         i++)
        unsetenv(args[i]);

    last_status = 0;
}

static int is_builtin(const char *cmd)
{
    if (!cmd)
        return 0;

    return
        !strcmp(cmd, "cd") ||
        !strcmp(cmd, "pwd") ||
        !strcmp(cmd, "echo") ||
        !strcmp(cmd, "mkdir") ||
        !strcmp(cmd, "touch") ||
        !strcmp(cmd, "cal") ||
        !strcmp(cmd, "clear") ||
        !strcmp(cmd, "whoami") ||
        !strcmp(cmd, "uname") ||
        !strcmp(cmd, "export") ||
        !strcmp(cmd, "unset") ||
        !strcmp(cmd, "history") ||
        !strcmp(cmd, "jobs") ||
        !strcmp(cmd, "fg") ||
        !strcmp(cmd, "bg") ||
        !strcmp(cmd, "help") ||
        !strcmp(cmd, "exit");
}

/* ---------------------------------------------------------- */
/* JOB CONTROL                                                  */
/* ---------------------------------------------------------- */

static int find_job(int id)
{
    for (int i = 0; i < job_count; i++)
        if (jobs[i].id == id)
            return i;

    return -1;
}

static int add_job(pid_t pgid,
                   const char *command,
                   int stopped)
{
    if (job_count >= MAX_JOBS)
        return -1;

    int id = job_count + 1;

    jobs[job_count].id = id;
    jobs[job_count].pgid = pgid;

    strncpy(
        jobs[job_count].command,
        command,
        SHELL_MAX_INPUT - 1
    );

    jobs[job_count]
        .command[SHELL_MAX_INPUT - 1] =
        '\0';

    jobs[job_count].running = !stopped;
    jobs[job_count].stopped = stopped;

    job_count++;

    return id;
}

static void remove_job(int index)
{
    if (index < 0 ||
        index >= job_count)
        return;

    memmove(
        &jobs[index],
        &jobs[index + 1],
        sizeof(Job) *
        (job_count - index - 1)
    );

    job_count--;
}

static void reap_jobs(void)
{
    for (int i = 0;
         i < job_count;) {

        int status;

        pid_t r = waitpid(
            -jobs[i].pgid,
            &status,
            WNOHANG |
            WUNTRACED |
            WCONTINUED
        );

        if (r == 0) {

            i++;
            continue;
        }

        if (r < 0) {

            if (errno == ECHILD) {

                printf(
                    "[%d] Done    %s\n",
                    jobs[i].id,
                    jobs[i].command
                );

                remove_job(i);

                continue;
            }

            i++;

            continue;
        }

        if (WIFSTOPPED(status)) {

            jobs[i].stopped = 1;
            jobs[i].running = 0;

            i++;
        }

        else if (WIFCONTINUED(status)) {

            jobs[i].stopped = 0;
            jobs[i].running = 1;

            i++;
        }

        else if (WIFEXITED(status) ||
                 WIFSIGNALED(status)) {

            printf(
                "[%d] Done    %s\n",
                jobs[i].id,
                jobs[i].command
            );

            remove_job(i);
        }

        else
            i++;
    }

    child_changed = 0;
}

static void builtin_jobs(void)
{
    reap_jobs();

    for (int i = 0;
         i < job_count;
         i++) {

        printf(
            "[%d] %s\t%s\n",
            jobs[i].id,
            jobs[i].stopped ?
            "Stopped" :
            "Running",
            jobs[i].command
        );
    }

    last_status = 0;
}

static int get_job_id(const char *s)
{
    if (!s)
        return -1;

    if (*s == '%')
        s++;

    char *end;

    long n = strtol(
        s,
        &end,
        10
    );

    if (*end ||
        n <= 0 ||
        n > INT_MAX)
        return -1;

    return (int)n;
}

/* ---------------------------------------------------------- */
/* HELP                                                         */
/* ---------------------------------------------------------- */

static void builtin_help(void)
{
    printf("\n");
    printf("hasini_2520030102 commands:\n\n");

    printf("  ls                 List files\n");
    printf("  pwd                Print directory\n");
    printf("  cd <dir>           Change directory\n");
    printf("  mkdir <dir>        Create directory\n");
    printf("  touch <file>       Create file\n");
    printf("  cat <file>         Display file\n");
    printf("  date               Display date/time\n");
    printf("  cal                Display calendar\n");
    printf("  echo <text>        Print text\n");
    printf("  clear              Clear screen\n");
    printf("  whoami             Show username\n");
    printf("  uname              Show system information\n");
    printf("  history            Show command history\n");
    printf("  export A=B         Set variable\n");
    printf("  unset A            Remove variable\n");
    printf("  jobs               Show jobs\n");
    printf("  fg %%1              Foreground job\n");
    printf("  bg %%1              Background job\n");
    printf("  exit               Exit shell\n\n");

    printf("Shell features:\n\n");

    printf("  command1 | command2\n");
    printf("  command < file\n");
    printf("  command > file\n");
    printf("  command >> file\n");
    printf("  command 2> file\n");
    printf("  command 2>&1\n");
    printf("  command &\n\n");

    last_status = 0;
}

/* ---------------------------------------------------------- */
/* FOREGROUND JOB                                               */
/* ---------------------------------------------------------- */

static void wait_for_job(pid_t pgid)
{
    int status;

    while (1) {

        pid_t r = waitpid(
            -pgid,
            &status,
            WUNTRACED
        );

        if (r < 0) {

            if (errno == EINTR)
                continue;

            if (errno == ECHILD)
                break;

            perror("waitpid");

            break;
        }

        if (WIFSTOPPED(status)) {

            int id =
                add_job(
                    pgid,
                    "stopped process",
                    1
                );

            if (id >= 0)
                printf(
                    "\n[%d] Stopped\n",
                    id
                );

            break;
        }

        if (WIFEXITED(status) ||
            WIFSIGNALED(status)) {

            int more = 0;

            while (
                waitpid(
                    -pgid,
                    &status,
                    WNOHANG
                ) > 0)
                more = 1;

            if (!more)
                break;
        }
    }
}

/* ---------------------------------------------------------- */
/* REDIRECTION                                                  */
/* ---------------------------------------------------------- */

static void apply_redirection(Command *cmd)
{
    if (cmd->input_file) {

        int fd = open(
            cmd->input_file,
            O_RDONLY
        );

        if (fd < 0) {

            perror(cmd->input_file);
            exit(1);
        }

        dup2(fd, STDIN_FILENO);

        close(fd);
    }

    if (cmd->output_file) {

        int fd = open(
            cmd->output_file,
            O_WRONLY |
            O_CREAT |
            O_TRUNC,
            0644
        );

        if (fd < 0) {

            perror(cmd->output_file);
            exit(1);
        }

        dup2(fd, STDOUT_FILENO);

        close(fd);
    }

    if (cmd->append_file) {

        int fd = open(
            cmd->append_file,
            O_WRONLY |
            O_CREAT |
            O_APPEND,
            0644
        );

        if (fd < 0) {

            perror(cmd->append_file);
            exit(1);
        }

        dup2(fd, STDOUT_FILENO);

        close(fd);
    }

    if (cmd->error_file) {

        int fd = open(
            cmd->error_file,
            O_WRONLY |
            O_CREAT |
            O_TRUNC,
            0644
        );

        if (fd < 0) {

            perror(cmd->error_file);
            exit(1);
        }

        dup2(fd, STDERR_FILENO);

        close(fd);
    }

    if (cmd->merge_error)
        dup2(
            STDOUT_FILENO,
            STDERR_FILENO
        );
}

/* ---------------------------------------------------------- */
/* EXTERNAL COMMAND EXECUTION                                  */
/* ---------------------------------------------------------- */

static void execute_external(
    Command *commands,
    int count,
    int background,
    const char *original)
{
    int pipes[MAX_PIPE - 1][2];

    for (int i = 0;
         i < count - 1;
         i++) {

        if (pipe(pipes[i]) < 0) {

            perror("pipe");
            return;
        }
    }

    pid_t pgid = 0;

    for (int i = 0;
         i < count;
         i++) {

        pid_t pid = fork();

        if (pid < 0) {

            perror("fork");
            return;
        }

        if (pid == 0) {

            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGTTIN, SIG_DFL);
            signal(SIGTTOU, SIG_DFL);
            signal(SIGCHLD, SIG_DFL);

            pid_t child_pgid =
                pgid ?
                pgid :
                getpid();

            setpgid(
                0,
                child_pgid
            );

            if (i > 0)
                dup2(
                    pipes[i - 1][0],
                    STDIN_FILENO
                );

            if (i < count - 1)
                dup2(
                    pipes[i][1],
                    STDOUT_FILENO
                );

            for (int p = 0;
                 p < count - 1;
                 p++) {

                close(pipes[p][0]);
                close(pipes[p][1]);
            }

            apply_redirection(
                &commands[i]
            );

            execvp(
                commands[i].argv[0],
                commands[i].argv
            );

            fprintf(
                stderr,
                "%s: %s: %s\n",
                SHELL_NAME,
                commands[i].argv[0],
                strerror(errno)
            );

            _exit(127);
        }

        if (pgid == 0)
            pgid = pid;

        setpgid(
            pid,
            pgid
        );
    }

    for (int i = 0;
         i < count - 1;
         i++) {

        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    if (background) {

        int id =
            add_job(
                pgid,
                original,
                0
            );

        if (id >= 0)
            printf(
                "[%d] %d\n",
                id,
                pgid
            );

        last_status = 0;

        return;
    }

    if (shell_interactive)
        tcsetpgrp(
            shell_terminal,
            pgid
        );

    wait_for_job(pgid);

    if (shell_interactive)
        tcsetpgrp(
            shell_terminal,
            shell_pgid
        );

    last_status = 0;
}

/* ---------------------------------------------------------- */
/* BUILTIN DISPATCH                                             */
/* ---------------------------------------------------------- */

static int run_builtin(Command *cmd)
{
    char **args = cmd->argv;

    if (!args[0])
        return 1;

    if (!strcmp(args[0], "cd"))
        builtin_cd(args);

    else if (!strcmp(args[0], "pwd"))
        builtin_pwd();

    else if (!strcmp(args[0], "echo"))
        builtin_echo(args);

    else if (!strcmp(args[0], "mkdir"))
        builtin_mkdir(args);

    else if (!strcmp(args[0], "touch"))
        builtin_touch(args);

    else if (!strcmp(args[0], "cal"))
        builtin_cal(args);

    else if (!strcmp(args[0], "clear"))
        builtin_clear();

    else if (!strcmp(args[0], "whoami"))
        builtin_whoami();

    else if (!strcmp(args[0], "uname"))
        builtin_uname();

    else if (!strcmp(args[0], "history"))
        show_history();

    else if (!strcmp(args[0], "export"))
        builtin_export(args);

    else if (!strcmp(args[0], "unset"))
        builtin_unset(args);

    else if (!strcmp(args[0], "jobs"))
        builtin_jobs();

    else if (!strcmp(args[0], "help"))
        builtin_help();

    else if (!strcmp(args[0], "exit")) {

        printf(
            "Exiting %s...\n",
            SHELL_NAME
        );

        fflush(stdout);

        return 2;
    }

    else
        return 0;

    return 1;
}

/* ---------------------------------------------------------- */
/* COMMAND EXECUTION                                            */
/* ---------------------------------------------------------- */

static void execute_line(char *line)
{
    Token tokens[MAX_TOKENS];

    memset(
        tokens,
        0,
        sizeof(tokens)
    );

    int token_count =
        tokenize(
            line,
            tokens
        );

    if (token_count <= 0)
        return;

    Command commands[MAX_PIPE];

    initialize_commands(commands);

    int command_count = 0;
    int background = 0;

    if (parse_commands(
            tokens,
            token_count,
            commands,
            &command_count,
            &background
        ) < 0) {

        free_commands(commands);
        free_tokens(
            tokens,
            token_count
        );

        return;
    }

    if (command_count == 1 &&
        is_builtin(
            commands[0].argv[0]
        ) &&
        !background &&
        !commands[0].input_file &&
        !commands[0].output_file &&
        !commands[0].append_file &&
        !commands[0].error_file &&
        !commands[0].merge_error) {

        int result =
            run_builtin(
                &commands[0]
            );

        if (result == 2) {

            free_commands(commands);

            free_tokens(
                tokens,
                token_count
            );

            printf("\n");

            exit(0);
        }
    }

    else {

        execute_external(
            commands,
            command_count,
            background,
            line
        );
    }

    free_commands(commands);

    free_tokens(
        tokens,
        token_count
    );
}

/* ---------------------------------------------------------- */
/* CLEANUP                                                      */
/* ---------------------------------------------------------- */

static void cleanup(void)
{
    for (int i = 0;
         i < history_count;
         i++)
        free(history[i]);
}

/* ---------------------------------------------------------- */
/* MAIN                                                         */
/* ---------------------------------------------------------- */

int main(void)
{
    setup_shell();

    atexit(cleanup);

    char input[SHELL_MAX_INPUT];

    printf("\n");
    printf("==============================================\n");
    printf("       %s - OSSP SHELL\n", SHELL_NAME);
    printf("==============================================\n");
    printf("You can directly run commands.\n");
    printf("Type 'exit' to close the shell.\n");
    printf("\n");

    while (1) {

        if (child_changed)
            reap_jobs();

        char cwd[PATH_MAX];
        char prompt[PATH_MAX + 64];

        if (getcwd(cwd, sizeof(cwd))) {

            snprintf(
                prompt,
                sizeof(prompt),
                "%s:%s$ ",
                SHELL_NAME,
                cwd
            );
        }

        else {

            snprintf(
                prompt,
                sizeof(prompt),
                "%s$ ",
                SHELL_NAME
            );
        }

        if (!read_line(
                prompt,
                input,
                sizeof(input)
            ))
            break;

        if (input[0] == '\0')
            continue;

        add_history(input);

        execute_line(input);
    }

    printf(
        "\nExiting %s...\n",
        SHELL_NAME
    );

    return 0;
}
