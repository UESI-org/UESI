#include <syscall.h>

/* System call with 0 arguments */
int64_t syscall0(uint64_t syscall_num) {
    int64_t ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(syscall_num)
        : "memory", "rcx", "r11"
    );
    return ret;
}

/* System call with 1 argument */
int64_t syscall1(uint64_t syscall_num, uint64_t arg1) {
    int64_t ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(syscall_num), "D"(arg1)
        : "memory", "rcx", "r11"
    );
    return ret;
}

/* System call with 2 arguments */
int64_t syscall2(uint64_t syscall_num, uint64_t arg1, uint64_t arg2) {
    int64_t ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(syscall_num), "D"(arg1), "S"(arg2)
        : "memory", "rcx", "r11"
    );
    return ret;
}

/* System call with 3 arguments */
int64_t syscall3(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    int64_t ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(syscall_num), "D"(arg1), "S"(arg2), "d"(arg3)
        : "memory", "rcx", "r11"
    );
    return ret;
}

int64_t open(const char *path, uint32_t flags, mode_t mode) {
    return syscall3(SYS_OPEN, (uint64_t)path, (uint64_t)flags, (uint64_t)mode);
}

int64_t close(int fd) {
    return syscall1(SYS_CLOSE, (uint64_t)fd);
}

int64_t read(int fd, void *buf, size_t count) {
    return syscall3(SYS_READ, (uint64_t)fd, (uint64_t)buf, (uint64_t)count);
}

int64_t write(int fd, const void *buf, size_t count) {
    return syscall3(SYS_WRITE, (uint64_t)fd, (uint64_t)buf, (uint64_t)count);
}

void exit(int status) {
    syscall1(SYS_EXIT, (uint64_t)status);
    __builtin_unreachable();
}

int sysinfo(struct sysinfo *info) {
    return (int)syscall1(SYS_SYSINFO, (uint64_t)info);
}