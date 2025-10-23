#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <stddef.h>

#define SYSCALL_READ  0
#define SYSCALL_WRITE 1
#define SYSCALL_EXIT  2

#define SYSCALL_INT 0x80

typedef struct {
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp, rsp;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, userrsp, ss;
} syscall_registers_t;

void syscall_init(void);
void syscall_handler(syscall_registers_t *regs);

int64_t sys_read(int fd, void *buf, size_t count);
int64_t sys_write(int fd, const void *buf, size_t count);
void sys_exit(int status);

#endif
