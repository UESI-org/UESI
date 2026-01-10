#ifndef _UNISTD_H_
#define _UNISTD_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/unistd.h>

#ifndef _GID_T_DEFINED_
#define _GID_T_DEFINED_
typedef __gid_t gid_t;
#endif

#ifndef _UID_T_DEFINED_
#define _UID_T_DEFINED_
typedef __uid_t uid_t;
#endif

#ifndef _OFF_T_DEFINED_
#define _OFF_T_DEFINED_
typedef __off_t off_t;
#endif

#ifndef _PID_T_DEFINED_
#define _PID_T_DEFINED_
typedef __pid_t pid_t;
#endif

#ifndef _SIZE_T_DEFINED_
#define _SIZE_T_DEFINED_
typedef __size_t size_t;
#endif

#ifndef _SSIZE_T_DEFINED_
#define _SSIZE_T_DEFINED_
typedef __ssize_t ssize_t;
#endif

#ifndef _USECONDS_T_DEFINED_
#define _USECONDS_T_DEFINED_
typedef __useconds_t useconds_t;
#endif

#ifndef _INTPTR_T_DEFINED_
#define _INTPTR_T_DEFINED_
typedef __intptr_t intptr_t;
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

#define _POSIX_VERSION 200809L
#define _POSIX2_VERSION 200809L
#define _XOPEN_VERSION 700

__BEGIN_DECLS

/* 1003.1-1990 */
void _exit(int) __dead;
int access(const char *, int);
unsigned int alarm(unsigned int);
int chdir(const char *);
int chown(const char *, uid_t, gid_t);
int close(int);
int dup(int);
int dup2(int, int);
int execl(const char *, const char *, ...) __attribute__((__sentinel__));
int execle(const char *, const char *, ...);
int execlp(const char *, const char *, ...) __attribute__((__sentinel__));
int execv(const char *, char *const *);
int execve(const char *, char *const *, char *const *);
int execvp(const char *, char *const *);
pid_t fork(void);
long fpathconf(int, int);
char *getcwd(char *, size_t) __warn_unused_result;
gid_t getegid(void);
uid_t geteuid(void);
gid_t getgid(void);
int getgroups(int, gid_t *);
char *getlogin(void);
pid_t getpgrp(void);
pid_t getpid(void);
pid_t getppid(void);
uid_t getuid(void);
int isatty(int);
int link(const char *, const char *);
off_t lseek(int, off_t, int);
long pathconf(const char *, int);
int pause(void);
int pipe(int *);
ssize_t read(int, void *, size_t);
int rmdir(const char *);
int setgid(gid_t);
int setpgid(pid_t, pid_t);
pid_t setsid(void);
int setuid(uid_t);
unsigned int sleep(unsigned int);
long sysconf(int);
pid_t tcgetpgrp(int);
int tcsetpgrp(int, pid_t);
char *ttyname(int);
int unlink(const char *);
ssize_t write(int, const void *, size_t);

/* 1003.2-1992 */
#if __POSIX_VISIBLE >= 199209 || __XPG_VISIBLE
size_t confstr(int, char *, size_t);
int getopt(int, char *const *, const char *);

extern char *optarg;  /* getopt(3) external variables */
extern int opterr;
extern int optind;
extern int optopt;
extern int optreset;
#endif

/* 1003.1c-1995 */
#if __POSIX_VISIBLE >= 199506 || __XPG_VISIBLE
int fsync(int);
int fdatasync(int);
int ftruncate(int, off_t);
int getlogin_r(char *, size_t);
#endif

/* 1003.1-2001 */
#if __POSIX_VISIBLE >= 200112 || __XPG_VISIBLE >= 420
int fchown(int, uid_t, gid_t);
int fchdir(int);
int gethostname(char *, size_t);
int readlink(const char *__restrict, char *__restrict, size_t);
int setegid(gid_t);
int seteuid(uid_t);
int symlink(const char *, const char *);
#endif

/* 1003.1-2008 */
#if __POSIX_VISIBLE >= 200809
int faccessat(int, const char *, int, int);
int fchownat(int, const char *, uid_t, gid_t, int);
int fexecve(int, char *const *, char *const *);
int linkat(int, const char *, int, const char *, int);
ssize_t readlinkat(int, const char *__restrict, char *__restrict, size_t);
int symlinkat(const char *, int, const char *);
int unlinkat(int, const char *, int);
#endif

/* X/Open Portability Guide, all versions */
#if __XPG_VISIBLE
int chroot(const char *);
int nice(int);
#endif

/* X/Open Portability Guide >= Issue 4 */
#if __XPG_VISIBLE >= 400
char *crypt(const char *, const char *);
int encrypt(char *, int);
long gethostid(void);
int lockf(int, int, off_t);
int setpgrp(pid_t, pid_t);
int setregid(gid_t, gid_t);
int setreuid(uid_t, uid_t);
void swab(const void *__restrict, void *__restrict, ssize_t);
void sync(void);
int truncate(const char *, off_t);
useconds_t ualarm(useconds_t, useconds_t);
int usleep(useconds_t);
pid_t vfork(void);
#endif

/* X/Open Portability Guide >= Issue 4 Version 2 */
#if __XPG_VISIBLE >= 420
ssize_t pread(int, void *, size_t, off_t);
ssize_t pwrite(int, const void *, size_t, off_t);
#endif

/* X/Open Portability Guide >= Issue 5 */
#if __XPG_VISIBLE >= 500
int brk(void *);
int getdtablesize(void);
int getpagesize(void);
char *getpass(const char *);
void *sbrk(intptr_t);
#endif

/* X/Open Portability Guide >= Issue 6 */
#if __XPG_VISIBLE >= 600
int lchown(const char *, uid_t, gid_t);
ssize_t pread(int, void *, size_t, off_t);
ssize_t pwrite(int, const void *, size_t, off_t);
#endif

#if __BSD_VISIBLE
int acct(const char *);
int closefrom(int);
int crypt_checkpass(const char *, const char *);
int crypt_newhash(const char *, const char *, char *, size_t);
int dup3(int, int, int);
int execvpe(const char *, char *const *, char *const *);
int getentropy(void *, size_t);
mode_t getmode(const void *, mode_t);
int getresgid(gid_t *, gid_t *, gid_t *);
int getresuid(uid_t *, uid_t *, uid_t *);
pid_t getthrid(void);
int issetugid(void);
int pipe2(int [2], int);
int pledge(const char *, const char *[]);
int rcmd(char **, int, const char *, const char *, const char *, int *);
int rcmd_af(char **, int, const char *, const char *, const char *, int *,
            int);
int rcmdsh(char **, int, const char *, const char *, const char *,
           const char *);
int reboot(int);
int revoke(const char *);
int rresvport(int *);
int rresvport_af(int *, int);
int ruserok(const char *, int, const char *, const char *);
int setgroups(int, const gid_t *);
void setmode(const void *);
int setresgid(gid_t, gid_t, gid_t);
int setresuid(uid_t, uid_t, uid_t);
void setproctitle(const char *, ...) __attribute__((__format__(__printf__, 1, 2)));
int unveil(const char *, const char *);
int vhangup(void);
#endif /* __BSD_VISIBLE */

__END_DECLS

#endif /* !_UNISTD_H_ */