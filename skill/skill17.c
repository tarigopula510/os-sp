#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>

struct Job {
    int id;
    pid_t pid;
};

int main() {
    struct Job jobs[5];
    int count = 0;

    for (int i = 0; i < 3; i++) {
        pid_t pid = fork();

        if (pid == 0) {
            printf("Job process %d running\n", i + 1);
            sleep(2);
            exit(0);
        }

        jobs[count].id = count + 1;
        jobs[count].pid = pid;
        count++;
    }

    printf("\nActive Jobs:\n");

    for (int i = 0; i < count; i++)
        printf("[%d] PID = %d\n",
               jobs[i].id, jobs[i].pid);

    for (int i = 0; i < count; i++)
        waitpid(jobs[i].pid, NULL, 0);

    printf("All jobs completed.\n");

    return 0;
}
