#include <sys/stat.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include "stdio_internal.h"

/* Error message table - minimal set for now */
const char *const sys_errlist[] = {
	"Success",				/* 0 - ESUCCESS */
	"Operation not permitted",		/* 1 - EPERM */
	"No such file or directory",		/* 2 - ENOENT */
	"No such process",			/* 3 - ESRCH */
	"Interrupted system call",		/* 4 - EINTR */
	"Input/output error",			/* 5 - EIO */
	"Device not configured",		/* 6 - ENXIO */
	"Argument list too long",		/* 7 - E2BIG */
	"Exec format error",			/* 8 - ENOEXEC */
	"Bad file descriptor",			/* 9 - EBADF */
	"No child processes",			/* 10 - ECHILD */
	"Resource deadlock avoided",		/* 11 - EDEADLK */
	"Cannot allocate memory",		/* 12 - ENOMEM */
	"Permission denied",			/* 13 - EACCES */
	"Bad address",				/* 14 - EFAULT */
	"Block device required",		/* 15 - ENOTBLK */
	"Device busy",				/* 16 - EBUSY */
	"File exists",				/* 17 - EEXIST */
	"Cross-device link",			/* 18 - EXDEV */
	"Operation not supported by device",	/* 19 - ENODEV */
	"Not a directory",			/* 20 - ENOTDIR */
	"Is a directory",			/* 21 - EISDIR */
	"Invalid argument",			/* 22 - EINVAL */
	"Too many open files in system",	/* 23 - ENFILE */
	"Too many open files",			/* 24 - EMFILE */
	"Inappropriate ioctl for device",	/* 25 - ENOTTY */
	"Text file busy",			/* 26 - ETXTBSY */
	"File too large",			/* 27 - EFBIG */
	"No space left on device",		/* 28 - ENOSPC */
	"Illegal seek",				/* 29 - ESPIPE */
	"Read-only file system",		/* 30 - EROFS */
	"Too many links",			/* 31 - EMLINK */
	"Broken pipe",				/* 32 - EPIPE */
};

const int sys_nerr = sizeof(sys_errlist) / sizeof(sys_errlist[0]);

void
perror(const char *s)
{
	const char *errstr;
	
	if (errno >= 0 && errno < sys_nerr)
		errstr = sys_errlist[errno];
	else
		errstr = "Unknown error";

	if (s != NULL && *s != '\0')
		fprintf(stderr, "%s: %s\n", s, errstr);
	else
		fprintf(stderr, "%s\n", errstr);
}

char *
strerror(int errnum)
{
	static char buf[64];

	if (errnum >= 0 && errnum < sys_nerr)
		return (char *)sys_errlist[errnum];

	snprintf(buf, sizeof(buf), "Unknown error: %d", errnum);
	return buf;
}

int
remove(const char *path)
{
	struct stat sb;

	if (stat(path, &sb) < 0)
		return -1;

	if (S_ISDIR(sb.st_mode))
		return rmdir(path);
	return unlink(path);
}

int
rename(const char *old, const char *new)
{
	/* POSIX rename() is not implemented as a syscall in this OS yet.
	 * We would need to implement sys_rename() in the kernel.
	 * For now, return ENOSYS.
	 */
	(void)old;
	(void)new;
	errno = ENOSYS;
	return -1;
}

#if __POSIX_VISIBLE >= 200809
int
renameat(int fromfd, const char *from, int tofd, const char *to)
{
	/* Not implemented */
	(void)fromfd;
	(void)from;
	(void)tofd;
	(void)to;
	errno = ENOSYS;
	return -1;
}
#endif

static int tmpfile_counter = 0;

FILE *
tmpfile(void)
{
	char name[L_tmpnam];
	int fd;
	FILE *fp;
	static unsigned int counter = 0;
	int attempts = 0;

	/* Try up to 1000 times to create unique file */
	while (attempts++ < 1000) {
		/* Create a unique temporary filename */
		snprintf(name, sizeof(name), "/tmp/tmp.%d.%u.%d",
			getpid(), counter++, attempts);

		/* Open with O_EXCL to ensure uniqueness (atomic) */
		fd = open(name, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
		if (fd >= 0)
			break;
		
		if (errno != EEXIST)
			return NULL;
	}
	
	if (fd < 0)
		return NULL;

	/* Immediately unlink so file is deleted when closed */
	(void)unlink(name);

	/* Create FILE structure */
	fp = fdopen(fd, "w+");
	if (fp == NULL) {
		close(fd);
		return NULL;
	}

	return fp;
}

static int tmpnam_counter = 0;

char *
tmpnam(char *s)
{
	static char buf[L_tmpnam];
	char *p = s ? s : buf;

	snprintf(p, L_tmpnam, "/tmp/tmp.%d.%d", getpid(), tmpnam_counter++);
	return p;
}

#if __XPG_VISIBLE >= 420
char *
tempnam(const char *dir, const char *pfx)
{
	char *name;
	const char *tmpdir;
	int ret;

	if (dir == NULL) {
		tmpdir = getenv("TMPDIR");
		if (tmpdir == NULL)
			tmpdir = P_tmpdir;
	} else
		tmpdir = dir;

	if (pfx == NULL)
		pfx = "tmp";

	/* Calculate required size */
	ret = snprintf(NULL, 0, "%s/%s.%d.%d", tmpdir, pfx, getpid(), tmpnam_counter);
	if (ret < 0)
		return NULL;

	/* Allocate memory */
	name = malloc(ret + 1);
	if (name == NULL)
		return NULL;

	/* Format the string */
	snprintf(name, ret + 1, "%s/%s.%d.%d", tmpdir, pfx, getpid(), tmpnam_counter++);
	return name;
}
#endif

/* Only define these functions if not using the macros from stdio.h */
#ifdef __cplusplus
int
fileno(FILE *fp)
{
	if (fp->_file < 0) {
		errno = EBADF;
		return -1;
	}
	return fp->_file;
}

void
clearerr(FILE *fp)
{
	fp->_flags &= ~(__SERR|__SEOF);
}

int
feof(FILE *fp)
{
	return (fp->_flags & __SEOF) != 0;
}

int
ferror(FILE *fp)
{
	return (fp->_flags & __SERR) != 0;
}
#endif

/* Thread locking stubs - not implemented in single-threaded kernel */
#if __POSIX_VISIBLE >= 199506
void
flockfile(FILE *fp)
{
	/* No-op in single-threaded environment */
	(void)fp;
}

int
ftrylockfile(FILE *fp)
{
	/* Always succeeds in single-threaded environment */
	(void)fp;
	return 0;
}

void
funlockfile(FILE *fp)
{
	/* No-op in single-threaded environment */
	(void)fp;
}
#endif