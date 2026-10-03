#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    char input[100];
    char variable[50];

    printf("Enter variable name: ");
    fgets(input, sizeof(input), stdin);

    input[strcspn(input, "\n")] = '\0';

    if (input[0] == '$')
        strcpy(variable, input + 1);
    else
        strcpy(variable, input);

    char *value = getenv(variable);

    if (value != NULL)
        printf("%s = %s\n", variable, value);
    else
        printf("Variable not found\n");

    return 0;
}
