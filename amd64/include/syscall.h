#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>

#define SYSCALL_EXIT 1
#define SYSCALL_FORK 2
#define SYSCALL_READ 3
#define SYSCALL_WRITE 4
#define SYSCALL_OPEN 5
#define SYSCALL_CLOSE 6
#define SYSCALL_CREAT 8
#define SYSCALL_BRK 17
#define SYSCALL_GETPID 20
#define SYSCALL_GETPPID 39
#define SYSCALL_DUP 41
#define SYSCALL_MUNMAP 73
#define SYSCALL_MPROTECT 74
#define SYSCALL_GETHOSTNAME 87
#define SYSCALL_DUP2 90
#define SYSCALL_FCNTL 92
#define SYSCALL_MKDIR 136
#define SYSCALL_RMDIR 137
#define SYSCALL_GETHOSTID 142
#define SYSCALL_UNAME 164
#define SYSCALL_MMAP 197
#define SYSCALL_LSEEK 199
#define SYSCALL_SYSINFO 214
#define SYSCALL_STAT 439
#define SYSCALL_FSTAT 440
#define SYSCALL_LSTAT 441
#define SYSCALL_OPENAT 468

#define SYSCALL_INT 0x80

typedef struct {
	uint64_t int_no, err_code;
	uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp, rsp;
	uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
	uint64_t rip, cs, rflags, userrsp, ss;
} syscall_registers_t;

struct sysinfo;
struct utsname;

void syscall_init(void);
void syscall_handler(syscall_registers_t *regs);

void sys_exit(int status) __attribute__((noreturn));
int64_t sys_fork(syscall_registers_t *regs);
int64_t sys_read(int fd, void *buf, size_t count);
int64_t sys_write(int fd, const void *buf, size_t count);
int64_t sys_open(const char *path, uint32_t flags, mode_t mode);
int64_t sys_close(int fd);
int64_t sys_creat(const char *path, mode_t mode);
int64_t sys_openat(int dirfd,
                   const char *pathname,
                   uint32_t flags,
                   mode_t mode);
int64_t sys_mkdir(const char *path, mode_t mode);
int64_t sys_rmdir(const char *path);
int64_t sys_fcntl(int fd, int cmd, uint64_t arg);
int64_t sys_dup(int oldfd);
int64_t sys_dup2(int oldfd, int newfd);
int64_t sys_stat(const char *path, struct stat *statbuf);
int64_t sys_fstat(int fd, struct stat *statbuf);
int64_t sys_lstat(const char *path, struct stat *statbuf);
int64_t sys_lseek(int fd, off_t offset, int whence);
void *sys_mmap(
    void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int64_t sys_brk(void *addr);
int64_t sys_getpid(void);
int64_t sys_gethostname(char *name, size_t len);
int64_t sys_getppid(void);
int64_t sys_gethostid(void);
int64_t sys_sysinfo(struct sysinfo *info);
int uname(struct utsname *buf);

#endif