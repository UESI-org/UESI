#include <syscall.h>
#include <sys/malloc.h>

int main() {
    write(1, "Hello, Userland!\n", 17);
    write(1, "Starting malloc test...\n", 24);
    
    write(1, "Calling malloc(100)...\n", 23);
    void *p1 = malloc(100);
    
    if (p1 == NULL) {
        write(1, "FAILED: malloc returned NULL\n", 29);
    } else {
        write(1, "SUCCESS: malloc worked!\n", 24);
        
        char *str = (char *)p1;
        str[0] = 'O';
        str[1] = 'K';
        str[2] = '\0';
        
        write(1, "Data: ", 6);
        write(1, str, 2);
        write(1, "\n", 1);
        
        write(1, "Calling free()...\n", 18);
        free(p1);
        write(1, "SUCCESS: free worked!\n", 22);
    }
    
    write(1, "\nAll malloc tests completed!\n", 29);
    exit(0);
}