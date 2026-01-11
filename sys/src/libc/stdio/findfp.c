#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include "stdio_internal.h"

/* Standard I/O file handles */
FILE __sF[FOPEN_MAX] = {
	/* stdin */
	{
		._p = NULL,
		._r = 0,
		._w = 0,
		._flags = __SRD,
		._file = STDIN_FILENO,
		._bf = { ._base = NULL, ._size = 0 },
		._lbfsize = 0,
	},
	/* stdout */
	{
		._p = NULL,
		._r = 0,
		._w = 0,
		._flags = __SWR | __SLBF,
		._file = STDOUT_FILENO,
		._bf = { ._base = NULL, ._size = 0 },
		._lbfsize = 0,
	},
	/* stderr */
	{
		._p = NULL,
		._r = 0,
		._w = 0,
		._flags = __SWR | __SNBF,
		._file = STDERR_FILENO,
		._bf = { ._base = NULL, ._size = 0 },
		._lbfsize = 0,
	},
};

/* Global pointer to standard FILE array */
FILE *__sF_ptr = __sF;

/* Find a free FILE structure */
FILE *
__sfp(void)
{
	FILE *fp;
	int n;

	for (fp = __sF, n = 0; n < FOPEN_MAX; n++, fp++)
		if (fp->_flags == 0)
			goto found;

	errno = EMFILE;
	return NULL;

found:
	fp->_flags = 1;		/* reserve this slot; caller sets real flags */
	fp->_p = NULL;		/* no current pointer */
	fp->_w = 0;		/* nothing to write */
	fp->_r = 0;		/* nothing to read */
	fp->_bf._base = NULL;	/* no buffer */
	fp->_bf._size = 0;
	fp->_lbfsize = 0;	/* not line buffered */
	fp->_file = -1;		/* no file */
	fp->_ub._base = NULL;	/* no ungetc buffer */
	fp->_ub._size = 0;
	fp->_lb._base = NULL;	/* no line buffer */
	fp->_lb._size = 0;
	fp->_blksize = 0;
	fp->_offset = 0;
	return fp;
}

/* Cleanup function for a FILE */
void
__scleanup(FILE *fp)
{
	if (fp->_flags == 0)
		return;		/* not active */

	(void)__sflush(fp);	/* flush any pending output */

	if (fp->_bf._base && (fp->_flags & __SMBF))
		free(fp->_bf._base);
	if (fp->_ub._base && fp->_ub._size > sizeof(fp->_ubuf))
		free(fp->_ub._base);
	if (fp->_lb._base)
		free(fp->_lb._base);

	fp->_flags = 0;		/* release this FILE */
}

/* Flush all open files */
int
__sflush_all(void)
{
	FILE *fp;
	int n, ret = 0;

	for (fp = __sF, n = 0; n < FOPEN_MAX; n++, fp++)
		if ((fp->_flags & (__SWR | __SRW)) != 0)
			if (__sflush(fp))
				ret = EOF;
	return ret;
}

/* Flush a single FILE */
int
__sflush(FILE *fp)
{
	unsigned char *p;
	int n, t;

	t = fp->_flags;
	if ((t & __SWR) == 0)
		return 0;

	if ((p = fp->_bf._base) == NULL)
		return 0;

	n = fp->_p - p;		/* write this much */
	if (n > 0) {
		if (fp->_write != NULL) {
			if ((*fp->_write)(fp->_cookie, (char *)p, n) != n) {
				fp->_flags |= __SERR;
				return EOF;
			}
		} else {
			if (write(fp->_file, p, n) != n) {
				fp->_flags |= __SERR;
				return EOF;
			}
		}
	}
	fp->_p = p;
	fp->_w = t & (__SLBF|__SNBF) ? 0 : fp->_bf._size;
	return 0;
}

/* Refill the input buffer */
int
__srefill(FILE *fp)
{
	/* make sure stdio is set up */
	if (fp->_r == 0)
		fp->_flags &= ~__SEOF;
	if (fp->_flags & __SEOF)
		return EOF;

	/* if not already reading, switch to reading */
	if ((fp->_flags & __SRD) == 0) {
		if (fp->_flags & __SWR) {
			if (__sflush(fp))
				return EOF;
			fp->_flags &= ~__SWR;
			fp->_w = 0;
			fp->_lbfsize = 0;
		}
		fp->_flags |= __SRD;
	}

	/* setup buffer if needed */
	if (fp->_bf._base == NULL)
		__smakebuf(fp);

	/*
	 * Before reading, we need to clear the ungetc buffer
	 * if we have been doing ungetc.
	 */
	if (HASUB(fp))
		FREEUB(fp);

	fp->_p = fp->_bf._base;
	if (fp->_read != NULL)
		fp->_r = (*fp->_read)(fp->_cookie, (char *)fp->_p, fp->_bf._size);
	else
		fp->_r = read(fp->_file, fp->_p, fp->_bf._size);

	if (fp->_r <= 0) {
		if (fp->_r == 0)
			fp->_flags |= __SEOF;
		else {
			fp->_r = 0;
			fp->_flags |= __SERR;
		}
		return EOF;
	}
	return 0;
}

/* Allocate a buffer for a FILE */
void
__smakebuf(FILE *fp)
{
	void *p;
	int flags;
	size_t size;
	int couldbetty;

	if (fp->_flags & (__SNBF|__SRD|__SWR))
		flags = fp->_flags;
	else
		flags = __swhatbuf(fp, &size, &couldbetty);

	if ((p = malloc(size)) == NULL) {
		fp->_flags |= __SNBF;
		fp->_bf._base = fp->_p = fp->_nbuf;
		fp->_bf._size = 1;
		return;
	}
	fp->_flags |= __SMBF;
	fp->_bf._base = fp->_p = p;
	fp->_bf._size = size;
	if (couldbetty && isatty(fp->_file))
		fp->_flags |= __SLBF;
}

/* Determine buffer size and type */
int
__swhatbuf(FILE *fp, size_t *bufsize, int *couldbetty)
{
	struct stat st;

	if (fp->_file < 0 || fstat(fp->_file, &st) < 0) {
		*couldbetty = 0;
		*bufsize = BUFSIZ;
		return __SNBF;
	}

	/* could be a tty if it's a character device */
	*couldbetty = S_ISCHR(st.st_mode);

	if (st.st_blksize > 0 && st.st_blksize < (1 << 20))
		*bufsize = st.st_blksize;
	else
		*bufsize = BUFSIZ;
	
	/*
	 * Optimize fseek() only if it is a regular file.
	 * (The test for __SOPT is mainly for historical reasons.)
	 */
	if ((fp->_flags & __SOPT) == 0) {
		if (S_ISREG(st.st_mode)) {
			fp->_flags |= __SOPT;
			fp->_blksize = st.st_blksize;
		} else
			fp->_flags |= __SNPT;
	}
	return __SOPT;
}

