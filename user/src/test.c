#include "syscall.h"

int main(void) {
    const char *msg = "Hello from userland!\n";
    write(1, msg, 21);
    
    const char *msg2 = "Userland is working!\n";
    write(1, msg2, 21);
    
    const char *msg3 = "About to exit...\n";
    write(1, msg3, 17);
    
    exit(0);
    __builtin_unreachable();
}