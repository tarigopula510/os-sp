#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main()
{
    int x = 100;

    printf("Before fork: x = %d\n", x);

    pid_t pid = fork();

    if (pid < 0)
    {
        printf("Fork failed\n");
        return 1;
    }

    if (pid == 0)
    {
        printf("Child Before = %d\n", x);

        x = 500;

        printf("Child After = %d\n", x);
    }
    else
    {
        wait(NULL);

        printf("Parent = %d\n", x);
    }

    return 0;
}
