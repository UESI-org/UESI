/* Syscall Wrapper */

#ifndef _USER_SYSCALL_H
#define _USER_SYSCALL_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define SYS_EXIT        1
#define SYS_READ        3
#define SYS_WRITE       4
#define SYS_OPEN        5
#define SYS_CLOSE       6
#define SYS_MMAP        9
#define SYS_MUNMAP      11
#define SYS_MPROTECT    74
#define SYS_GETHOSTNAME 87
#define SYS_GETHOSTID   142
#define SYS_SYSINFO     214

#define PROT_NONE       0x00
#define PROT_READ       0x01
#define PROT_WRITE      0x02
#define PROT_EXEC       0x04

#define MAP_SHARED      0x0001
#define MAP_PRIVATE     0x0002
#define MAP_FIXED       0x0010
#define MAP_ANONYMOUS   0x0020
#define MAP_ANON        MAP_ANONYMOUS

#define MAP_FAILED      ((void *)-1)

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

extern int errno;

void exit(int status) __attribute__((noreturn));
int64_t read(int fd, void *buf, size_t count);
int64_t write(int fd, const void *buf, size_t count);
int64_t open(const char *path, uint32_t flags, mode_t mode);
int64_t close(int fd);
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int munmap(void *addr, size_t length);
int mprotect(void *addr, size_t len, int prot);
int gethostname(char *name, size_t len);
int gethostid(void);
int sysinfo(struct sysinfo *info);

#endif