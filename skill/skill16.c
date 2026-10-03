#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>

int main() {
    pid_t pid = fork();

    if (pid == 0) {
        printf("Background process started\n");

        for (int i = 1; i <= 5; i++) {
            printf("Working... %d\n", i);
            sleep(1);
        }

        printf("Background process completed\n");
        exit(0);
    }
    else if (pid > 0) {
        printf("Parent continues immediately.\n");
        printf("Background PID: %d\n", pid);

        wait(NULL);
        printf("Parent finished.\n");
    }
    else {
        perror("fork");
    }

    return 0;
}
