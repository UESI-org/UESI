#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "stdio_internal.h"

extern int __sflush(FILE *);
extern int __sflush_all(void);

int
fflush(FILE *fp)
{
	if (fp == NULL)
		return __sflush_all();
	if ((fp->_flags & (__SWR | __SRW)) == 0) {
		errno = EBADF;
		return EOF;
	}
	return __sflush(fp);
}

int
setvbuf(FILE *fp, char *buf, int mode, size_t size)
{
	int ret, flags;
	size_t iosize;
	int ttyflag;

	/*
	 * Verify arguments.  The `int' limit on `size' is due to this
	 * particular implementation.  Note, buf and size are ignored
	 * when setting _IONBF.
	 */
	if (mode != _IONBF)
		if ((mode != _IOFBF && mode != _IOLBF) || (int)size < 0)
			return EOF;

	/*
	 * Write current buffer, if any.  Discard unread input (including
	 * ungetc data), cancel line buffering, and free old buffer if
	 * malloc()ed.  We also clear any eof condition, as if this were
	 * a seek.
	 */
	ret = 0;
	(void)__sflush(fp);
	if (HASUB(fp))
		FREEUB(fp);
	fp->_r = fp->_lbfsize = 0;
	flags = fp->_flags;
	if (flags & __SMBF)
		free(fp->_bf._base);
	flags &= ~(__SLBF | __SNBF | __SMBF | __SOPT | __SNPT | __SEOF);

	/* If setting unbuffered mode, skip all the hard work. */
	if (mode == _IONBF)
		goto nbf;

	/*
	 * Find optimal I/O size for seek optimization.  This also returns
	 * a `tty flag' to suggest that we check isatty(fd), but we do not
	 * care since our caller told us how to buffer.
	 */
	flags |= __swhatbuf(fp, &iosize, &ttyflag);
	if (size == 0) {
		buf = NULL;	/* force local allocation */
		size = iosize;
	}

	/* Allocate buffer if needed. */
	if (buf == NULL) {
		if ((buf = malloc(size)) == NULL) {
			/*
			 * Unable to honor user's request.  We will use the
			 * existing buffer (if any) and go into line-buffered
			 * mode for the lowest memory usage.
			 */
			ret = EOF;
			if (fp->_bf._base == NULL) {
				/* No existing buffer, must use our emergency buffer */
nbf:
				flags |= __SNBF;
				fp->_bf._base = fp->_p = fp->_nbuf;
				fp->_bf._size = 1;
			} else {
				/* We already have a buffer, just make it line-buffered */
				flags |= __SLBF;
				fp->_bf._size = BUFSIZ;
			}
		} else {
			flags |= __SMBF;
			fp->_bf._base = fp->_p = (unsigned char *)buf;
			fp->_bf._size = size;
		}
	} else {
		/*
		 * User supplied buffer.  Verify that it's properly aligned
		 * and sized.
		 */
		fp->_bf._base = fp->_p = (unsigned char *)buf;
		fp->_bf._size = size;
	}

	/*
	 * Patch up write count and pointer if not in write mode.
	 */
	if (mode == _IOLBF) {
		flags |= __SLBF;
		fp->_lbfsize = -fp->_bf._size;
		fp->_w = 0;
	} else {
		fp->_w = flags & __SNBF ? 0 : size;
	}

	fp->_flags = flags;
	return ret;
}

void
setbuf(FILE *fp, char *buf)
{
	(void)setvbuf(fp, buf, buf ? _IOFBF : _IONBF, BUFSIZ);
}

#if __BSD_VISIBLE
void
setbuffer(FILE *fp, char *buf, int size)
{
	(void)setvbuf(fp, buf, buf ? _IOFBF : _IONBF, (size_t)size);
}

int
setlinebuf(FILE *fp)
{
	return setvbuf(fp, NULL, _IOLBF, (size_t)0);
}
#endif

