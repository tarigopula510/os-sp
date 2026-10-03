#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

int main() {
    char input[200];

    while (1) {
        char cwd[200];

        getcwd(cwd, sizeof(cwd));
        printf("%s> ", cwd);

        fgets(input, sizeof(input), stdin);
        input[strcspn(input, "\n")] = '\0';

        if (strcmp(input, "exit") == 0)
            break;

        if (strcmp(input, "cd") == 0) {
            chdir(getenv("HOME"));
        }
        else if (strncmp(input, "cd ", 3) == 0) {
            if (chdir(input + 3) != 0)
                perror("cd");
        }
        else {
            printf("Use: cd <directory>\n");
        }
    }

    return 0;
}
