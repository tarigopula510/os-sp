#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdlib.h>

int main() {
    printf("OSSP Final Demonstration\n");
    printf("------------------------\n");

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        printf("Child process created.\n");
        printf("Child PID: %d\n", getpid());

        sleep(1);

        printf("\nExecuting pipeline...\n");

        int pipefd[2];
        pipe(pipefd);

        pid_t p = fork();

        if (p == 0) {
            dup2(pipefd[1], STDOUT_FILENO);

            close(pipefd[0]);
            close(pipefd[1]);

            execlp("printf", "printf",
                   "Linux\nOperating\nSystems\n",
                   NULL);

            perror("exec");
            exit(1);
        }

        dup2(pipefd[0], STDIN_FILENO);

        close(pipefd[0]);
        close(pipefd[1]);

        waitpid(p, NULL, 0);

        execlp("wc", "wc", "-l", NULL);

        perror("exec");
        exit(1);
    }
    else {
        printf("Parent process created.\n");
        printf("Parent PID: %d\n", getpid());

        waitpid(pid, NULL, 0);

        printf("\nFinal OSSP demonstration completed successfully.\n");
    }

    return 0;
}
