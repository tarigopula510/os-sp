#include <stdio.h>
#include <stdlib.h>

int main() {
    int *ptr = malloc(5 * sizeof(int));

    if (ptr == NULL) {
        perror("malloc");
        return 1;
    }

    for (int i = 0; i < 5; i++)
        ptr[i] = i + 1;

    printf("Array: ");

    for (int i = 0; i < 5; i++)
        printf("%d ", ptr[i]);

    printf("\n");

    free(ptr);
    ptr = NULL;

    printf("Memory released successfully.\n");

    return 0;
}
