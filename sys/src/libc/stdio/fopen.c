#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "stdio_internal.h"

extern FILE *__sfp(void);
extern int __sflags(const char *, int *);

FILE *
fopen(const char *file, const char *mode)
{
	FILE *fp;
	int f;
	int flags, oflags;

	if ((flags = __sflags(mode, &oflags)) == 0)
		return NULL;
	if ((fp = __sfp()) == NULL)
		return NULL;
	if ((f = open(file, oflags, 0666)) < 0) {
		fp->_flags = 0;			/* release */
		return NULL;
	}
	
	fp->_file = f;
	fp->_flags = flags;
	fp->_cookie = fp;
	fp->_read = NULL;
	fp->_write = NULL;
	fp->_seek = NULL;
	fp->_close = NULL;

	/*
	 * When opening in append mode, even though we use O_APPEND,
	 * we need to seek to the end so that ftell() gets the right
	 * answer.  If the user then alters the seek pointer, or
	 * the file extends, this will fail, but there is not much
	 * we can do about this.  (We could set a flag so that fseek
	 * adjusts the offset within the stdio buffer, but this loses
	 * the advantage of using lseek directly.)
	 */
	if (oflags & O_APPEND)
		(void)__sseek((void *)fp, (fpos_t)0, SEEK_END);
	return fp;
}

FILE *
fdopen(int fd, const char *mode)
{
	FILE *fp;
	int flags, oflags, fdflags;

	if ((flags = __sflags(mode, &oflags)) == 0)
		return NULL;

	/* Make sure the mode the user wants is a subset of the actual mode. */
	if ((fdflags = fcntl(fd, F_GETFL, 0)) < 0)
		return NULL;
	
	/* O_ACCMODE is O_RDONLY, O_WRONLY, and O_RDWR */
	fdflags &= O_ACCMODE;
	if (fdflags != O_RDWR && (fdflags != (oflags & O_ACCMODE))) {
		errno = EINVAL;
		return NULL;
	}

	if ((fp = __sfp()) == NULL)
		return NULL;
	
	fp->_flags = flags;
	/*
	 * If opened for appending, set append flag. Also, if opened for
	 * appending and writing, seek to the end now so that ftell() returns
	 * the right value.
	 */
	if (oflags & O_APPEND) {
		fp->_flags |= __SAPP;
		if (oflags & O_WRONLY)
			(void)__sseek((void *)fp, (fpos_t)0, SEEK_END);
	}
	
	fp->_file = fd;
	fp->_cookie = fp;
	fp->_read = NULL;
	fp->_write = NULL;
	fp->_seek = NULL;
	fp->_close = NULL;
	return fp;
}

FILE *
freopen(const char *file, const char *mode, FILE *fp)
{
	int f;
	int flags, oflags;
	int oldfd = -1;

	if ((flags = __sflags(mode, &oflags)) == 0) {
		(void)fclose(fp);
		return NULL;
	}

	/*
	 * Remember whether the stream was open to begin with, and which file
	 * descriptor (if any) was associated with it.
	 */
	if (fp->_flags == 0) {
		fp->_flags = __SEOF;	/* hold on to it */
		oldfd = -1;
	} else {
		/* flush the stream */
		if (fp->_flags & __SWR)
			(void)__sflush(fp);
		
		/* Remember old fd for NULL file case */
		oldfd = fp->_file;
		
		/* Close via close function if available */
		if (fp->_close != NULL)
			(void)(*fp->_close)(fp->_cookie);
		else if (fp->_file >= 0)
			(void)close(fp->_file);
	}

	if (file != NULL) {
		/* Opening a new file */
		if ((f = open(file, oflags, 0666)) < 0) {
			fp->_flags = 0;	/* release */
			return NULL;
		}
	} else {
		/*
		 * Re-open the same file with new mode.
		 * We need to get a new fd with the new flags.
		 */
		if (oldfd < 0) {
			errno = EBADF;
			fp->_flags = 0;
			return NULL;
		}
		
		/* Dup the old fd to get a new one we can apply new flags to */
#ifdef F_DUPFD_CLOEXEC
		f = fcntl(oldfd, F_DUPFD_CLOEXEC, 0);
#else
		f = fcntl(oldfd, F_DUPFD, 0);
		if (f >= 0)
			(void)fcntl(f, F_SETFD, FD_CLOEXEC);
#endif
		
		/* Now close the old fd */
		if (oldfd >= 0)
			close(oldfd);
		
		if (f < 0) {
			fp->_flags = 0;
			return NULL;
		}
		
		/* Set the access mode on the new fd */
		int fdflags = fcntl(f, F_GETFL, 0);
		if (fdflags >= 0) {
			fdflags = (fdflags & ~O_ACCMODE) | (oflags & O_ACCMODE);
			(void)fcntl(f, F_SETFL, fdflags);
		}
	}

	/*
	 * Set up the new file descriptor.
	 */
	fp->_file = f;
	fp->_flags = flags;
	fp->_cookie = fp;
	fp->_read = NULL;
	fp->_write = NULL;
	fp->_seek = NULL;
	fp->_close = NULL;
	
	/* Reset buffer state */
	fp->_p = NULL;
	fp->_r = 0;
	fp->_w = 0;
	fp->_ub._base = NULL;
	fp->_ub._size = 0;

	if (oflags & O_APPEND)
		(void)__sseek((void *)fp, (fpos_t)0, SEEK_END);
	
	return fp;
}

int
__sflags(const char *mode, int *optr)
{
	int ret, m, o;

	switch (*mode++) {
	case 'r':	/* open for reading */
		ret = __SRD;
		m = O_RDONLY;
		o = 0;
		break;

	case 'w':	/* open for writing */
		ret = __SWR;
		m = O_WRONLY;
		o = O_CREAT | O_TRUNC;
		break;

	case 'a':	/* open for appending */
		ret = __SWR;
		m = O_WRONLY;
		o = O_CREAT | O_APPEND;
		break;

	default:	/* illegal mode */
		errno = EINVAL;
		return 0;
	}

	/* [rwa]\+ or [rwa]b\+ means read and write */
	while (*mode != '\0') {
		switch (*mode++) {
		case 'b':
			/* 'b' (binary) is ignored */
			break;
		case '+':
			ret = __SRW;
			m = (m & ~O_ACCMODE) | O_RDWR;
			break;
		case 'e':
			o |= O_CLOEXEC;
			break;
		case 'x':
			if (m == O_RDONLY) {
				errno = EINVAL;
				return 0;
			}
			o |= O_EXCL;
			break;
		default:
			/* Ignore unknown flags */
			break;
		}
	}

	*optr = m | o;
	return ret;
}

fpos_t
__sseek(void *cookie, fpos_t offset, int whence)
{
	FILE *fp = cookie;
	off_t ret;
	
	ret = lseek(fp->_file, (off_t)offset, whence);
	if (ret == -1L)
		return EOF;
	return (fpos_t)ret;
}

int
__sread(void *cookie, char *buf, int n)
{
	FILE *fp = cookie;
	return (int)read(fp->_file, buf, (size_t)n);
}

int
__swrite(void *cookie, const char *buf, int n)
{
	FILE *fp = cookie;
	return (int)write(fp->_file, buf, (size_t)n);
}

int
__sclose(void *cookie)
{
	FILE *fp = cookie;
	return close(fp->_file);
}