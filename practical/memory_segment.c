#include <stdio.h>
#include <stdlib.h>

int global = 100;
static int s = 50;

int main()
{
    int stack = 10;
    int *heap = (int *)malloc(sizeof(int));

    if (heap == NULL)
    {
        printf("Memory allocation failed\n");
        return 1;
    }

    *heap = 20;

    printf("Code Segment  : %p\n", (void *)main);
    printf("Global Segment: %p\n", (void *)&global);
    printf("Static Segment: %p\n", (void *)&s);
    printf("Heap Segment  : %p\n", (void *)heap);
    printf("Stack Segment : %p\n", (void *)&stack);

    free(heap);

    return 0;
}
