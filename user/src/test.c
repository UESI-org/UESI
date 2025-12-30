#include <sys/stat.h>

#include <syscall.h>
#include <fcntl.h>

int main() {
    int ret;
    
    write(1, "Creating directories...\n", 24);
    
    if (mkdir("/dev", 0755) < 0) {
        write(1, "Warning: /dev might already exist\n", 34);
    }
    
    if (mkdir("/tmp", 0755) < 0) {
        write(1, "Warning: /tmp might already exist\n", 34);
    }
    
    write(1, "Creating regular file...\n", 25);
    if (mknod("/tmp/file.txt", S_IFREG | 0644, 0) < 0) {
        write(1, "Failed to create file\n", 22);
        return 1;
    }
    write(1, "  /tmp/file.txt created\n", 24);
    
    write(1, "Creating character device...\n", 29);
    dev_t dev = makedev(1, 3);
    if (mknod("/dev/mynull", S_IFCHR | 0666, dev) < 0) {
        write(1, "Failed to create device\n", 24);
        return 1;
    }
    write(1, "  /dev/mynull created (major=1, minor=3)\n", 41);
    
    write(1, "Creating FIFO...\n", 17);
    if (mknod("/tmp/mypipe", S_IFIFO | 0644, 0) < 0) {
        write(1, "Failed to create FIFO\n", 22);
        return 1;
    }
    write(1, "  /tmp/mypipe created\n", 22);
    
    write(1, "Creating block device...\n", 25);
    dev_t blkdev = makedev(8, 0);
    if (mknod("/dev/myblock", S_IFBLK | 0660, blkdev) < 0) {
        write(1, "Failed to create block device\n", 30);
        return 1;
    }
    write(1, "  /dev/myblock created (major=8, minor=0)\n", 42);
    
    write(1, "\nVerifying created nodes...\n", 28);
    struct stat st;
    
    if (stat("/tmp/file.txt", &st) == 0) {
        write(1, "  /tmp/file.txt: ", 17);
        if (S_ISREG(st.st_mode)) {
            write(1, "regular file OK\n", 16);
        }
    }
    
    if (stat("/dev/mynull", &st) == 0) {
        write(1, "  /dev/mynull: ", 15);
        if (S_ISCHR(st.st_mode)) {
            write(1, "character device OK\n", 20);
        }
    }
    
    if (stat("/tmp/mypipe", &st) == 0) {
        write(1, "  /tmp/mypipe: ", 15);
        if (S_ISFIFO(st.st_mode)) {
            write(1, "FIFO OK\n", 8);
        }
    }
    
    if (stat("/dev/myblock", &st) == 0) {
        write(1, "  /dev/myblock: ", 16);
        if (S_ISBLK(st.st_mode)) {
            write(1, "block device OK\n", 16);
        }
    }
    
    write(1, "\nAll mknod operations succeeded!\n", 33);
    return 0;
}