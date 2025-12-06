#include <syscall.h>

int main() {
    char buf[256];
    struct sysinfo si;
    char hostname[256];
    int hostid;
    
    write(1, "Hello, Userland!\n", 17);
    write(1, "Testing BSD-style syscalls...\n", 30);
    
    if (sysinfo(&si) == 0) {
        write(1, "System info retrieved successfully\n", 35);
    }
    
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        write(1, "Hostname: ", 10);
        int len = 0;
        while (hostname[len] && len < 256) len++;
        write(1, hostname, len);
        write(1, "\n", 1);
    }
    
    hostid = gethostid();
    write(1, "Host ID retrieved\n", 18);
    
    write(1, "Enter your name: ", 17);
    int n = read(0, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        write(1, "Hello, ", 7);
        write(1, buf, n);
    }
    
    write(1, "\nAll syscalls tested!\n", 22);
    
    exit(0);
}