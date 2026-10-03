#include <stdio.h>
#include <signal.h>
#include <unistd.h>

void handler(int signal) {
    printf("\nSIGINT received: %d\n", signal);
    printf("Process is still running.\n");
}

int main() {
    signal(SIGINT, handler);

    printf("Press Ctrl+C to send SIGINT.\n");

    while (1) {
        printf("Running...\n");
        sleep(2);
    }

    return 0;
}
