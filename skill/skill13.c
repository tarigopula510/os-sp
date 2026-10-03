#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>

int main() {
    pid_t pid = fork();

    if (pid == 0) {
        int fd = open("input.txt", O_RDONLY);

        if (fd < 0) {
            perror("input.txt");
            exit(1);
        }

        dup2(fd, STDIN_FILENO);
        close(fd);

        execlp("cat", "cat", NULL);

        perror("exec");
        exit(1);
    }
    else {
        wait(NULL);
    }

    return 0;
}
