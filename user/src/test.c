#include <fcntl.h>
#include <syscall.h>
#include <errno.h>

#define NULL ((void *)0)
#define EEXIST 17  /* From errno.h - directory already exists is OK */

/* Simple string length */
static size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

/* Helper to write a string */
static void print(const char *msg) {
    write(1, msg, strlen(msg));
}

int main(void) {
    int fd;
    const char *filename = "/tmp/fcntl_test.txt";
    
    print("=== fcntl.h Test Program ===\n\n");
    
    /* Create /tmp directory first */
    print("Setup: Creating /tmp directory\n");
    if (mkdir("/tmp", 0755) < 0) {
        if (errno == EEXIST) {
            print("  /tmp already exists (OK)\n");
        } else {
            print("  FAIL: Could not create /tmp\n");
            return 1;
        }
    } else {
        print("  OK: Created /tmp\n");
    }
    print("\n");
    
    /* Test 1: Create and write to a new file */
    print("Test 1: O_CREAT | O_WRONLY | O_TRUNC\n");
    fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        print("  FAIL: Could not create file\n");
        return 1;
    }
    write(fd, "Hello, World!\n", 14);
    print("  OK: Wrote to file\n");
    close(fd);
    
    /* Test 2: Append to existing file */
    print("\nTest 2: O_APPEND | O_WRONLY\n");
    fd = open(filename, O_APPEND | O_WRONLY, 0);
    if (fd < 0) {
        print("  FAIL: Could not open for append\n");
        return 1;
    }
    write(fd, "Appended line.\n", 15);
    print("  OK: Appended to file\n");
    close(fd);
    
    /* Test 3: Test O_EXCL (should fail if file exists) */
    print("\nTest 3: O_CREAT | O_EXCL (should fail)\n");
    fd = open(filename, O_CREAT | O_EXCL | O_WRONLY, 0644);
    if (fd < 0) {
        print("  OK: Correctly rejected O_EXCL on existing file\n");
    } else {
        print("  FAIL: O_EXCL should have failed!\n");
        close(fd);
    }
    
    /* Test 4: fcntl - get file flags */
    print("\nTest 4: fcntl F_GETFL\n");
    fd = open(filename, O_RDWR | O_APPEND, 0);
    if (fd < 0) {
        print("  FAIL: Could not open file\n");
        return 1;
    }
    
    int flags = fcntl(fd, F_GETFL);
    if (flags < 0) {
        print("  FAIL: F_GETFL failed\n");
    } else {
        print("  OK: Got flags = ");
        /* Simple hex output */
        char buf[20];
        int i = 0;
        unsigned int uflags = (unsigned int)flags;
        do {
            int digit = uflags % 16;
            buf[i++] = digit < 10 ? '0' + digit : 'a' + digit - 10;
            uflags /= 16;
        } while (uflags > 0);
        buf[i++] = 'x';
        buf[i++] = '0';
        
        /* Reverse the string */
        for (int j = 0; j < i/2; j++) {
            char tmp = buf[j];
            buf[j] = buf[i-j-1];
            buf[i-j-1] = tmp;
        }
        buf[i++] = '\n';
        write(1, buf, i);
    }
    
    /* Test 5: fcntl - clear O_APPEND flag */
    print("\nTest 5: fcntl F_SETFL (clear O_APPEND)\n");
    flags = flags & ~O_APPEND;
    if (fcntl(fd, F_SETFL, flags) < 0) {
        print("  FAIL: F_SETFL failed\n");
    } else {
        print("  OK: Cleared O_APPEND flag\n");
    }
    
    close(fd);
    
    /* Test 6: O_CLOEXEC */
    print("\nTest 6: O_CLOEXEC\n");
    fd = open(filename, O_RDONLY | O_CLOEXEC, 0);
    if (fd < 0) {
        print("  FAIL: Could not open with O_CLOEXEC\n");
    } else {
        int fd_flags = fcntl(fd, F_GETFD);
        if (fd_flags & FD_CLOEXEC) {
            print("  OK: FD_CLOEXEC is set\n");
        } else {
            print("  FAIL: FD_CLOEXEC not set!\n");
        }
        close(fd);
    }
    
    print("\n=== All tests completed ===\n");
    return 0;
}