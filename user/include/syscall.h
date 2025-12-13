/* Syscall Wrapper */

#ifndef _USER_SYSCALL_H
#define _USER_SYSCALL_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define SYSCALL_EXIT        1
#define SYSCALL_FORK        2
#define SYSCALL_READ        3
#define SYSCALL_WRITE       4
#define SYSCALL_OPEN        5
#define SYSCALL_CLOSE       6
#define SYSCALL_CREAT       8
#define SYSCALL_BRK         17
#define SYSCALL_GETPID      20
#define SYSCALL_GETPPID     39
#define SYSCALL_DUP         41
#define SYSCALL_MUNMAP      73
#define SYSCALL_MPROTECT    74
#define SYSCALL_GETHOSTNAME 87
#define SYSCALL_DUP2        90
#define SYSCALL_FCNTL       92
#define SYSCALL_GETHOSTID   142
#define SYSCALL_UNAME       164
#define SYSCALL_MMAP        197
#define SYSCALL_LSEEK       199
#define SYSCALL_SYSINFO     214
#define SYSCALL_STAT        439
#define SYSCALL_FSTAT       440
#define SYSCALL_LSTAT       441
#define SYSCALL_OPENAT      468

#define PROT_NONE           0x00
#define PROT_READ           0x01
#define PROT_WRITE          0x02
#define PROT_EXEC           0x04

#define MAP_SHARED          0x0001
#define MAP_PRIVATE         0x0002
#define MAP_FIXED           0x0010
#define MAP_ANONYMOUS       0x0020
#define MAP_ANON            MAP_ANONYMOUS

#define MAP_FAILED          ((void *)-1)

struct sysinfo {
    int64_t uptime;
    uint64_t loads[3];
    uint64_t totalram;
    uint64_t freeram;
    uint64_t sharedram;
    uint64_t bufferram;
    uint64_t totalswap;
    uint64_t freeswap;
    uint16_t procs;
    uint64_t totalhigh;
    uint64_t freehigh;
    uint32_t mem_unit;
    char _f[20-2*sizeof(uint64_t)-sizeof(uint32_t)];
};

struct stat;
struct utsname;
extern int errno;

void exit(int status) __attribute__((noreturn));
pid_t fork(void);
int64_t read(int fd, void *buf, size_t count);
int64_t write(int fd, const void *buf, size_t count);
int64_t open(const char *path, uint32_t flags, mode_t mode);
int64_t close(int fd);
int64_t creat(const char *path, mode_t mode);
int64_t openat(int dirfd, const char *pathname, uint32_t flags, mode_t mode);
int fcntl(int fd, int cmd, ...);
int stat(const char *path, struct stat *buf);
int fstat(int fd, struct stat *buf);
int lstat(const char *path, struct stat *buf);
off_t lseek(int fd, off_t offset, int whence);
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int munmap(void *addr, size_t length);
void *brk(void *addr);
void *sbrk(intptr_t increment);
pid_t getpid(void);
int mprotect(void *addr, size_t len, int prot);
int gethostname(char *name, size_t len);
pid_t getppid(void);
int gethostid(void);
int sysinfo(struct sysinfo *info);

#endif