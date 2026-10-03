#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>

int main() {
    pid_t pid = fork();

    if (pid == 0) {
        int fd = open("combined.txt",
                      O_WRONLY | O_CREAT | O_TRUNC,
                      0644);

        if (fd < 0) {
            perror("open");
            exit(1);
        }

        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);

        close(fd);

        printf("Normal output\n");
        fprintf(stderr, "Error output\n");

        exit(0);
    }

    wait(NULL);

    printf("Both outputs redirected to combined.txt\n");

    return 0;
}
