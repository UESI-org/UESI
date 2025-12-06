#include <syscall.h>

int main() {
    char buf[256];
    struct sysinfo si;
    char hostname[256];
    int hostid;
    
    write(1, "Hello, Userland!\n", 17);
    
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
    
    write(1, "\n=== Testing mmap() ===\n", 24);
    write(1, "Allocating 4KB with mmap...\n", 28);
    
    errno = 0;
    void *addr = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    if (addr == MAP_FAILED) {
        write(1, "FAILED: mmap returned MAP_FAILED, errno=", 41);
        char ebuf[8];
        int e = errno;
        int i = 0;
        if (e == 0) {
            write(1, "0", 1);
        } else {
            while (e > 0 && i < 7) {
                ebuf[i++] = '0' + (e % 10);
                e /= 10;
            }
            for (int j = i - 1; j >= 0; j--) {
                write(1, &ebuf[j], 1);
            }
        }
        write(1, "\n", 1);
    } else {
        write(1, "SUCCESS: Memory allocated\n", 26);
        
        char *ptr = (char *)addr;
        ptr[0] = 'H';
        ptr[1] = 'e';
        ptr[2] = 'l';
        ptr[3] = 'l';
        ptr[4] = 'o';
        ptr[5] = '!';
        ptr[6] = '\0';
        
        write(1, "Wrote to mmap'd memory: ", 24);
        write(1, ptr, 6);
        write(1, "\n", 1);
        write(1, "Unmapping memory...\n", 20);
        if (munmap(addr, 4096) == 0) {
            write(1, "SUCCESS: munmap worked\n", 23);
        } else {
            write(1, "FAILED: munmap failed\n", 22);
        }
    }
    
    write(1, "\nEnter your name: ", 18);
    int n = read(0, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        write(1, "Hello, ", 7);
        write(1, buf, n);
    }
    
    write(1, "\nAll syscalls tested!\n", 22);
    exit(0);
}