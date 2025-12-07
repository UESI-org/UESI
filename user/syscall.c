#include <syscall.h>
#include <errno.h>

int errno = 0;

/* System call with 0 arguments */
static inline int64_t syscall0(uint64_t syscall_num) {
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
static inline int64_t syscall1(uint64_t syscall_num, uint64_t arg1) {
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
static inline int64_t syscall2(uint64_t syscall_num, uint64_t arg1, uint64_t arg2) {
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
static inline int64_t syscall3(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    int64_t ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(syscall_num), "D"(arg1), "S"(arg2), "d"(arg3)
        : "memory", "rcx", "r11"
    );
    return ret;
}

/* System call with 6 arguments - needed for mmap */
static inline int64_t syscall6(uint64_t syscall_num, uint64_t arg1, uint64_t arg2,
                                uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    int64_t ret;
    register uint64_t r10 asm("r10") = arg4;
    register uint64_t r8 asm("r8") = arg5;
    register uint64_t r9 asm("r9") = arg6;
    
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(syscall_num), "D"(arg1), "S"(arg2), "d"(arg3),
          "r"(r10), "r"(r8), "r"(r9)
        : "memory", "rcx", "r11"
    );
    return ret;
}

static inline int64_t handle_syscall_result(int64_t ret) {
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    errno = 0;
    return ret;
}

int64_t open(const char *path, uint32_t flags, mode_t mode) {
    int64_t ret = syscall3(SYSCALL_OPEN, (uint64_t)path, (uint64_t)flags, (uint64_t)mode);
    return handle_syscall_result(ret);
}

int64_t close(int fd) {
    int64_t ret = syscall1(SYSCALL_CLOSE, (uint64_t)fd);
    return handle_syscall_result(ret);
}

int64_t read(int fd, void *buf, size_t count) {
    int64_t ret = syscall3(SYSCALL_READ, (uint64_t)fd, (uint64_t)buf, (uint64_t)count);
    return handle_syscall_result(ret);
}

int64_t write(int fd, const void *buf, size_t count) {
    int64_t ret = syscall3(SYSCALL_WRITE, (uint64_t)fd, (uint64_t)buf, (uint64_t)count);
    return handle_syscall_result(ret);
}

int stat(const char *path, struct stat *buf) {
    int64_t ret = syscall2(SYSCALL_STAT, (uint64_t)path, (uint64_t)buf);
    return (int)handle_syscall_result(ret);
}

int fstat(int fd, struct stat *buf) {
    int64_t ret = syscall2(SYSCALL_FSTAT, (uint64_t)fd, (uint64_t)buf);
    return (int)handle_syscall_result(ret);
}

int lstat(const char *path, struct stat *buf) {
    int64_t ret = syscall2(SYSCALL_LSTAT, (uint64_t)path, (uint64_t)buf);
    return (int)handle_syscall_result(ret);
}

pid_t fork(void) {
    int64_t ret = syscall0(SYSCALL_FORK);
    
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    
    return (pid_t)ret;
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    int64_t ret = syscall6(SYSCALL_MMAP, (uint64_t)addr, (uint64_t)length,
                           (uint64_t)prot, (uint64_t)flags,
                           (uint64_t)fd, (uint64_t)offset);
    
    if (ret < 0 && ret >= -4096) {
        errno = (int)(-ret);
        return MAP_FAILED;
    }
    
    errno = 0;
    return (void *)ret;
}

int munmap(void *addr, size_t length) {
    int64_t ret = syscall2(SYSCALL_MUNMAP, (uint64_t)addr, (uint64_t)length);
    return (int)handle_syscall_result(ret);
}

pid_t getpid(void) {
    return (pid_t)syscall0(SYSCALL_GETPID);
}

int mprotect(void *addr, size_t len, int prot) {
    int64_t ret = syscall3(SYSCALL_MPROTECT, (uint64_t)addr, (uint64_t)len, (uint64_t)prot);
    return (int)handle_syscall_result(ret);
}

void exit(int status) {
    syscall1(SYSCALL_EXIT, (uint64_t)status);
    __builtin_unreachable();
}

int sysinfo(struct sysinfo *info) {
    int64_t ret = syscall1(SYSCALL_SYSINFO, (uint64_t)info);
    return (int)handle_syscall_result(ret);
}

int gethostname(char *name, size_t len) {
    int64_t ret = syscall2(SYSCALL_GETHOSTNAME, (uint64_t)name, (uint64_t)len);
    return (int)handle_syscall_result(ret);
}

pid_t getppid(void) {
    return (pid_t)syscall0(SYSCALL_GETPPID);
}

int gethostid(void) {
    /* gethostid returns a host ID value, not an error code */
    return (int)syscall0(SYSCALL_GETHOSTID);
}