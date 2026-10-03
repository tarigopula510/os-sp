#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

int main() {
    pid_t pid = fork();

    if (pid == 0) {
        while (1) {
            printf("Child is running...\n");
            sleep(1);
        }
    }
    else {
        sleep(3);

        printf("Stopping child...\n");
        kill(pid, SIGSTOP);

        sleep(3);

        printf("Resuming child...\n");
        kill(pid, SIGCONT);

        sleep(3);

        printf("Terminating child...\n");
        kill(pid, SIGTERM);

        wait(NULL);
    }

    return 0;
}
