#include <syscall.h>

int main() {
    write(1, "Hello, userland!", 16);
    exit(0);
}