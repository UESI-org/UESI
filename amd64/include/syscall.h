#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#define SYSCALL_EXIT        1
#define SYSCALL_READ        3
#define SYSCALL_WRITE       4
#define SYSCALL_OPEN        5
#define SYSCALL_CLOSE       6
#define SYSCALL_MMAP        9
#define SYSCALL_MUNMAP      11
#define SYSCALL_MPROTECT    74
#define SYSCALL_GETHOSTNAME 87
#define SYSCALL_GETHOSTID   142
#define SYSCALL_SYSINFO     214

#define SYSCALL_INT 0x80

typedef struct {
    uint64_t int_no, err_code;
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp, rsp;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t rip, cs, rflags, userrsp, ss;
} syscall_registers_t;

struct sysinfo;

void syscall_init(void);
void syscall_handler(syscall_registers_t *regs);

void sys_exit(int status) __attribute__((noreturn));
int64_t sys_read(int fd, void *buf, size_t count);
int64_t sys_write(int fd, const void *buf, size_t count);
int64_t sys_open(const char *path, uint32_t flags, mode_t mode);
int64_t sys_close(int fd);
int64_t sys_gethostname(char *name, size_t len);
int64_t sys_gethostid(void);
int64_t sys_sysinfo(struct sysinfo *info);

#endif