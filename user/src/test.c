#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(void) {
    int secret;
    int guess;
    int attempts = 0;

    /* seed RNG once */
    srand(time(NULL));

    /* random number between 1 and 100 */
    secret = (rand() % 100) + 1;

    printf("Guess the number (1-100)\n");

    while (1) {
        printf("> ");
        if (scanf("%d", &guess) != 1) {
            printf("Invalid input\n");
            return 1;
        }

        attempts++;

        if (guess < secret) {
            printf("Too low\n");
        } else if (guess > secret) {
            printf("Too high\n");
        } else {
            printf("Correct! Attempts: %d\n", attempts);
            break;
        }
    }

    return 0;
}
