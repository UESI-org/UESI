#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "stdio_internal.h"

extern int __sflush(FILE *);
extern void __scleanup(FILE *);

int
fclose(FILE *fp)
{
	int r;

	if (fp->_flags == 0) {		/* not open! */
		errno = EBADF;
		return EOF;
	}

	r = fp->_flags & __SWR ? __sflush(fp) : 0;

	if (fp->_close != NULL && (*fp->_close)(fp->_cookie) < 0)
		r = EOF;
	else if (fp->_file >= 0 && close(fp->_file) < 0)
		r = EOF;

	/* Free buffer if it was malloced */
	if (fp->_bf._base && (fp->_flags & __SMBF))
		free(fp->_bf._base);
	if (fp->_ub._base && fp->_ub._size > sizeof(fp->_ubuf))
		free(fp->_ub._base);
	if (fp->_lb._base)
		free(fp->_lb._base);

	fp->_flags = 0;		/* Release this FILE for reuse. */
	fp->_r = fp->_w = 0;	/* Mess up if reaccessed. */
	fp->_file = -1;
	return r;
}