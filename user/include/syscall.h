/* User Syscalls wrapper */

#ifndef _USER_SYSCALL_H
#define _USER_SYSCALL_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define SYS_READ  0
#define SYS_WRITE 1
#define SYS_EXIT  2
#define SYS_OPEN  3
#define SYS_CLOSE 4

int64_t syscall0(uint64_t syscall_num);
int64_t syscall1(uint64_t syscall_num, uint64_t arg1);
int64_t syscall2(uint64_t syscall_num, uint64_t arg1, uint64_t arg2);
int64_t syscall3(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3);

int64_t read(int fd, void *buf, size_t count);
int64_t write(int fd, const void *buf, size_t count);
void exit(int status) __attribute__((noreturn));
int64_t open(const char *path, uint32_t flags, mode_t mode);
int64_t close(int fd);

#endif