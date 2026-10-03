#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>

int main() {
    char input[200];

    while (1) {
        printf("ossp> ");
        fflush(stdout);

        fgets(input, sizeof(input), stdin);
        input[strcspn(input, "\n")] = '\0';

        if (strcmp(input, "exit") == 0)
            break;

        if (strcmp(input, "pwd") == 0) {
            char cwd[200];
            getcwd(cwd, sizeof(cwd));
            printf("%s\n", cwd);
            continue;
        }

        if (strncmp(input, "cd ", 3) == 0) {
            if (chdir(input + 3) != 0)
                perror("cd");
            continue;
        }

        pid_t pid = fork();

        if (pid == 0) {
            execlp("/bin/sh", "sh", "-c", input, NULL);
            perror("Command execution failed");
            exit(1);
        }
        else if (pid > 0) {
            wait(NULL);
        }
        else {
            perror("fork");
        }
    }

    return 0;
}
