#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

int main() {
    pid_t pid = fork();

    if (pid == 0) {
        setpgid(0, 0);

        while (1) {
            printf("Child process group running...\n");
            sleep(1);
        }
    }
    else {
        setpgid(pid, pid);

        sleep(3);

        printf("Stopping process group...\n");
        kill(-pid, SIGSTOP);

        sleep(3);

        printf("Continuing process group...\n");
        kill(-pid, SIGCONT);

        sleep(3);

        printf("Terminating process group...\n");
        kill(-pid, SIGTERM);

        wait(NULL);
    }

    return 0;
}
