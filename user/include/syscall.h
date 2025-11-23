/* User Syscalls wrapper */

#ifndef _USER_SYSCALL_H
#define _USER_SYSCALL_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define SYS_READ    0
#define SYS_WRITE   1
#define SYS_EXIT    2
#define SYS_OPEN    3
#define SYS_CLOSE   4
#define SYS_SYSINFO 5

struct sysinfo {
    int64_t uptime;             /* Seconds since boot */
    uint64_t loads[3];          /* 1, 5, and 15 minute load averages */
    uint64_t totalram;          /* Total usable main memory size */
    uint64_t freeram;           /* Available memory size */
    uint64_t sharedram;         /* Amount of shared memory */
    uint64_t bufferram;         /* Memory used by buffers */
    uint64_t totalswap;         /* Total swap space size */
    uint64_t freeswap;          /* Swap space still available */
    uint16_t procs;             /* Number of current processes */
    uint64_t totalhigh;         /* Total high memory size */
    uint64_t freehigh;          /* Available high memory size */
    uint32_t mem_unit;          /* Memory unit size in bytes */
    char _f[20-2*sizeof(uint64_t)-sizeof(uint32_t)];  /* Padding */
};

int64_t syscall0(uint64_t syscall_num);
int64_t syscall1(uint64_t syscall_num, uint64_t arg1);
int64_t syscall2(uint64_t syscall_num, uint64_t arg1, uint64_t arg2);
int64_t syscall3(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3);

int64_t read(int fd, void *buf, size_t count);
int64_t write(int fd, const void *buf, size_t count);
void exit(int status) __attribute__((noreturn));
int64_t open(const char *path, uint32_t flags, mode_t mode);
int64_t close(int fd);
int sysinfo(struct sysinfo *info);

#endif