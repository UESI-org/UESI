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

	if ((flags = __sflags(mode, &oflags)) == 0) {
		(void)fclose(fp);
		return NULL;
	}

	/*
	 * There are actually programs that depend on being able to "freopen"
	 * descriptors that weren't originally open.  Keep this from breaking.
	 * Remember whether the stream was open to begin with, and which file
	 * descriptor (if any) was associated with it.  If it was attached to
	 * a descriptor, defer closing it; freopen("/dev/stdin", "r", stdin)
	 * should work.  This is unnecessary if it was not a Unix file.
	 */
	if (fp->_flags == 0) {
		fp->_flags = __SEOF;	/* hold on to it */
	} else {
		/* flush the stream; ANSI doesn't require this. */
		if (fp->_flags & __SWR)
			(void)__sflush(fp);
		/* if close is NULL, closing is a no-op, hence pointless */
		if (fp->_close != NULL)
			(void)(*fp->_close)(fp->_cookie);
		else if (fp->_file >= 0)
			(void)close(fp->_file);
	}

	if (file != NULL) {
		if ((f = open(file, oflags, 0666)) < 0) {
			fp->_flags = 0;	/* release */
			return NULL;
		}
	} else {
		/*
		 * Re-open the current file with the new mode. This is used
		 * primarily when changing the mode of a stream.
		 */
		if ((f = fcntl(fp->_file, F_DUPFD_CLOEXEC, 0)) < 0) {
			fp->_flags = 0;
			return NULL;
		}
	}

	/*
	 * Set up the new file descriptor, which will be used in preference
	 * to the old one (if the old one was open).
	 */
	fp->_file = f;
	fp->_flags = flags;
	fp->_cookie = fp;
	fp->_read = NULL;
	fp->_write = NULL;
	fp->_seek = NULL;
	fp->_close = NULL;
	
	/* reset buffer state */
	fp->_p = NULL;
	fp->_r = 0;
	fp->_w = 0;
	fp->_ub._base = NULL;

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