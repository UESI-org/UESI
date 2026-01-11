#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int count;
    int *arr;
    int sum = 0;

    printf("How many numbers? ");
    scanf("%d", &count);

    if (count <= 0) {
        printf("Invalid count\n");
        return 1;
    }

    arr = malloc(sizeof(int) * count);
    if (arr == NULL) {
        printf("malloc failed\n");
        return 1;
    }

    for (int i = 0; i < count; i++) {
        printf("Number %d: ", i + 1);
        scanf(" %d", &arr[i]);
        sum += arr[i];
    }

    printf("Sum: %d\n", sum);
    printf("Average: %d\n", sum / count);

    free(arr);
    return 0;
}
