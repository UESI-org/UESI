/* User-space syscall wrapper header */

#ifndef _USER_SYSCALL_H
#define _USER_SYSCALL_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/time.h>

#define SYSCALL_EXIT 1
#define SYSCALL_FORK 2
#define SYSCALL_READ 3
#define SYSCALL_WRITE 4
#define SYSCALL_OPEN 5
#define SYSCALL_CLOSE 6
#define SYSCALL_CREAT 8
#define SYSCALL_LINK 9
#define SYSCALL_UNLINK 10
#define SYSCALL_CHDIR 12
#define SYSCALL_FCHDIR 13
#define SYSCALL_CHMOD 15
#define SYSCALL_CHOWN 16
#define SYSCALL_BRK 17
#define SYSCALL_GETPID 20
#define SYSCALL_ACCESS 33
#define SYSCALL_GETPPID 39
#define SYSCALL_DUP 41
#define SYSCALL_SYMLINK 57
#define SYSCALL_READLINK 58
#define SYSCALL_MUNMAP 73
#define SYSCALL_MPROTECT 74
#define SYSCALL_GETHOSTNAME 87
#define SYSCALL_DUP2 90
#define SYSCALL_FCNTL 92
#define SYSCALL_RENAME 128
#define SYSCALL_MKDIR 136
#define SYSCALL_RMDIR 137
#define SYSCALL_GETHOSTID 142
#define SYSCALL_UNAME 164
#define SYSCALL_MMAP 197
#define SYSCALL_LSEEK 199
#define SYSCALL_TRUNCATE 200
#define SYSCALL_FTRUNCATE 201
#define SYSCALL_SYSINFO 214
#define SYSCALL_GETDENTS64 220
#define SYSCALL_GETDENTS 272
#define SYSCALL_GETCWD 296
#define SYSCALL_GETTIMEOFDAY 418
#define SYSCALL_CLOCK_GETTIME 427
#define SYSCALL_CLOCK_GETRES 429
#define SYSCALL_NANOSLEEP 430
#define SYSCALL_STAT 439
#define SYSCALL_FSTAT 440
#define SYSCALL_LSTAT 441
#define SYSCALL_MKNOD 450
#define SYSCALL_OPENAT 468

#define PROT_NONE 0x00
#define PROT_READ 0x01
#define PROT_WRITE 0x02
#define PROT_EXEC 0x04

#define MAP_SHARED 0x0001
#define MAP_PRIVATE 0x0002
#define MAP_FIXED 0x0010
#define MAP_ANONYMOUS 0x0020
#define MAP_ANON MAP_ANONYMOUS

#define MAP_FAILED ((void *)-1)

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
	char _f[20 - 2 * sizeof(uint64_t) - sizeof(uint32_t)];
};

struct stat;
struct utsname;

extern int errno;

void _exit(int status) __attribute__((noreturn));
pid_t fork(void);
pid_t getpid(void);
pid_t getppid(void);

int64_t read(int fd, void *buf, size_t count);
int64_t write(int fd, const void *buf, size_t count);
int open(const char *path, int flags, ...);
int close(int fd);
int creat(const char *path, mode_t mode);
int openat(int dirfd, const char *pathname, int flags, ...);
off_t lseek(int fd, off_t offset, int whence);

int dup(int oldfd);
int dup2(int oldfd, int newfd);
int fcntl(int fd, int cmd, ...);

int mkdir(const char *path, mode_t mode);
int rmdir(const char *path);
char *getcwd(char *buf, size_t size);
int chdir(const char *path);
int fchdir(int fd);
int getdents(int fd, void *dirp, size_t count);

int unlink(const char *path);
int link(const char *oldpath, const char *newpath);
int symlink(const char *target, const char *linkpath);
ssize_t readlink(const char *path, char *buf, size_t bufsiz);
int rename(const char *oldpath, const char *newpath);
int truncate(const char *path, off_t length);
int ftruncate(int fd, off_t length);
int mknod(const char *path, mode_t mode, dev_t dev);

int stat(const char *path, struct stat *buf);
int fstat(int fd, struct stat *buf);
int lstat(const char *path, struct stat *buf);
int access(const char *pathname, int mode);
int chmod(const char *path, mode_t mode);
int chown(const char *pathname, uid_t owner, gid_t group);

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int munmap(void *addr, size_t length);
int mprotect(void *addr, size_t len, int prot);
void *brk(void *addr);
void *sbrk(intptr_t increment);

int gethostname(char *name, size_t len);
int gethostid(void);
int uname(struct utsname *buf);
int sysinfo(struct sysinfo *info);

int gettimeofday(struct timeval *tv, struct timezone *tz);
int clock_gettime(clockid_t clock_id, struct timespec *tp);
int clock_getres(clockid_t clock_id, struct timespec *res);
int nanosleep(const struct timespec *req, struct timespec *rem);
unsigned int sleep(unsigned int seconds);
int usleep(useconds_t usec);

int isatty(int fd);

#endif /* _USER_SYSCALL_H */