#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>

int main() {
    pid_t pid = fork();

    if (pid == 0) {
        int out = open("output.txt",
                       O_WRONLY | O_CREAT | O_TRUNC,
                       0644);

        int err = open("error.txt",
                       O_WRONLY | O_CREAT | O_TRUNC,
                       0644);

        if (out < 0 || err < 0) {
            perror("open");
            exit(1);
        }

        dup2(out, STDOUT_FILENO);
        dup2(err, STDERR_FILENO);

        close(out);
        close(err);

        printf("This is normal output.\n");
        fprintf(stderr, "This is error output.\n");

        exit(0);
    }

    wait(NULL);

    printf("Check output.txt and error.txt\n");

    return 0;
}
