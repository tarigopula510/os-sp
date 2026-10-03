#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main() {
    char input[200];

    while (1) {
        printf("ossp> ");
        fgets(input, sizeof(input), stdin);

        input[strcspn(input, "\n")] = '\0';

        if (strcmp(input, "exit") == 0)
            break;

        if (strcmp(input, "pwd") == 0) {
            char cwd[200];
            getcwd(cwd, sizeof(cwd));
            printf("%s\n", cwd);
        }
        else if (strncmp(input, "export ", 7) == 0) {
            char *data = input + 7;
            char *equal = strchr(data, '=');

            if (equal != NULL) {
                *equal = '\0';
                setenv(data, equal + 1, 1);
            }
        }
        else {
            printf("Available commands: pwd, export, exit\n");
        }
    }

    return 0;
}
