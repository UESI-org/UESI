#include <syscall.h>

int
main(int argc, char* argv[])
{
    char buffer[128];
    int64_t bytes_read;
    
    write(1, "Enter some text: ", 17);
    bytes_read = read(0, buffer, sizeof(buffer) - 1);
    
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        
        write(1, "You entered: ", 13);
        write(1, buffer, bytes_read);
        write(1, "\n", 1);
    } else if (bytes_read == 0) {
        write(1, "EOF reached\n", 12);
    } else {
        write(1, "Read error\n", 11);
    }
    
    return 0;
}