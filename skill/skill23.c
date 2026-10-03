#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>

int main() {
    int p1[2], p2[2];

    pipe(p1);
    pipe(p2);

    pid_t p1id = fork();

    if (p1id == 0) {
        dup2(p1[1], STDOUT_FILENO);

        close(p1[0]);
        close(p1[1]);
        close(p2[0]);
        close(p2[1]);

        execlp("seq", "seq", "1", "100", NULL);

        perror("exec");
        exit(1);
    }

    pid_t p2id = fork();

    if (p2id == 0) {
        dup2(p1[0], STDIN_FILENO);
        dup2(p2[1], STDOUT_FILENO);

        close(p1[0]);
        close(p1[1]);
        close(p2[0]);
        close(p2[1]);

        execlp("cat", "cat", NULL);

        perror("exec");
        exit(1);
    }

    pid_t p3id = fork();

    if (p3id == 0) {
        dup2(p2[0], STDIN_FILENO);

        close(p1[0]);
        close(p1[1]);
        close(p2[0]);
        close(p2[1]);

        execlp("wc", "wc", "-l", NULL);

        perror("exec");
        exit(1);
    }

    close(p1[0]);
    close(p1[1]);
    close(p2[0]);
    close(p2[1]);

    waitpid(p1id, NULL, 0);
    waitpid(p2id, NULL, 0);
    waitpid(p3id, NULL, 0);

    printf("Large pipeline completed successfully.\n");

    return 0;
}
