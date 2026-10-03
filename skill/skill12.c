#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    int pipefd[2];
    char buffer[100];

    pipe(pipefd);

    pid_t pid = fork();

    if (pid == 0) {
        close(pipefd[1]);

        read(pipefd[0], buffer, sizeof(buffer));

        printf("Child received: %s\n", buffer);

        close(pipefd[0]);
    }
    else {
        close(pipefd[0]);

        char message[] = "Hello from parent";

        write(pipefd[1], message, sizeof(message));

        close(pipefd[1]);

        wait(NULL);
    }

    return 0;
}
