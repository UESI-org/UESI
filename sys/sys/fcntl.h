#ifndef _SYS_FCNTL_H_
#define _SYS_FCNTL_H_

#include <sys/cdefs.h>
#ifndef _KERNEL
#include <sys/types.h>
#endif

__BEGIN_DECLS

#define O_RDONLY 0x0000  /* open for reading only */
#define O_WRONLY 0x0001  /* open for writing only */
#define O_RDWR 0x0002    /* open for reading and writing */
#define O_ACCMODE 0x0003 /* mask for above modes */

#define O_NONBLOCK 0x0004 /* no delay */
#define O_APPEND 0x0008   /* set append mode */
#define O_CREAT 0x0100    /* create if nonexistent */
#define O_TRUNC 0x0400    /* truncate to zero length */
#define O_EXCL 0x0800     /* error if already exists */

#define O_SYNC 0x0080       /* synchronous writes */
#define O_NOFOLLOW 0x0200   /* if path is a symlink, don't follow */
#define O_NOCTTY 0x8000     /* don't assign controlling terminal */
#define O_CLOEXEC 0x10000   /* atomically set FD_CLOEXEC */
#define O_DIRECTORY 0x20000 /* fail if not a directory */

#ifdef _KERNEL
#define FREAD 0x0001
#define FWRITE 0x0002

#define FFLAGS(oflags) (((oflags) & ~O_ACCMODE) | (((oflags) + 1) & O_ACCMODE))
#define OFLAGS(fflags) (((fflags) & ~O_ACCMODE) | (((fflags) - 1) & O_ACCMODE))

#define FMASK (FREAD | FWRITE | O_APPEND | O_SYNC | O_NONBLOCK)
#define FCNTLFLAGS (O_APPEND | O_SYNC | O_NONBLOCK)

#define FAPPEND O_APPEND
#define FFSYNC O_SYNC
#define FNONBLOCK O_NONBLOCK
#endif

#define F_DUPFD 0          /* duplicate file descriptor */
#define F_GETFD 1          /* get file descriptor flags */
#define F_SETFD 2          /* set file descriptor flags */
#define F_GETFL 3          /* get file status flags */
#define F_SETFL 4          /* set file status flags */
#define F_GETOWN 5         /* get SIGIO/SIGURG proc/pgrp */
#define F_SETOWN 6         /* set SIGIO/SIGURG proc/pgrp */
#define F_GETLK 7          /* get record locking information */
#define F_SETLK 8          /* set record locking information */
#define F_SETLKW 9         /* F_SETLK; wait if blocked */
#define F_DUPFD_CLOEXEC 10 /* duplicate with FD_CLOEXEC set */

#define FD_CLOEXEC 1 /* close-on-exec flag */

#define F_RDLCK 1 /* shared or read lock */
#define F_UNLCK 2 /* unlock */
#define F_WRLCK 3 /* exclusive or write lock */

#ifdef _KERNEL
#define F_WAIT 0x010  /* Wait until lock is granted */
#define F_FLOCK 0x020 /* Use flock(2) semantics for lock */
#define F_POSIX 0x040 /* Use POSIX semantics for lock */
#endif

struct flock {
	off_t l_start;  /* starting offset */
	off_t l_len;    /* len = 0 means until end of file */
	pid_t l_pid;    /* lock owner */
	short l_type;   /* lock type: read/write, etc. */
	short l_whence; /* type of l_start */
};

#define LOCK_SH 0x01 /* shared file lock */
#define LOCK_EX 0x02 /* exclusive file lock */
#define LOCK_NB 0x04 /* don't block when locking */
#define LOCK_UN 0x08 /* unlock file */

#define AT_FDCWD -100

#define AT_EACCESS 0x01
#define AT_SYMLINK_NOFOLLOW 0x02
#define AT_SYMLINK_FOLLOW 0x04
#define AT_REMOVEDIR 0x08

#ifdef _KERNEL
extern int64_t open(const char *path, uint32_t flags, mode_t mode);
extern int64_t creat(const char *path, mode_t mode);
extern int fcntl(int fd, int cmd, ...);
#if __POSIX_VISIBLE >= 200809
extern int64_t openat(int dirfd, const char *pathname, uint32_t flags, mode_t mode);
#endif
#endif

__END_DECLS

#endif