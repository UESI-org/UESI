#include <stdint.h>

extern void tty_putchar(char c);

void putchar_(char c) {
    tty_putchar(c);
}