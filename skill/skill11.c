#include <stdio.h>
#include <string.h>

int main() {
    char input[300];
    char *command;

    printf("Enter pipeline: ");
    fgets(input, sizeof(input), stdin);

    printf("\nPipeline commands:\n");

    command = strtok(input, "|");

    int count = 1;

    while (command != NULL) {
        command[strcspn(command, "\n")] = '\0';

        printf("%d. %s\n", count++, command);

        command = strtok(NULL, "|");
    }

    return 0;
}
