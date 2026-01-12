#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stdio_internal.h"

extern int __srefill(FILE *);

int
ungetc(int c, FILE *fp)
{
	if (c == EOF)
		return EOF;

	if ((fp->_flags & __SRD) == 0) {
		/*
		 * Not currently reading, so set up for reading.
		 */
		if (fp->_flags & __SWR) {
			if (__sflush(fp))
				return EOF;
			fp->_flags &= ~__SWR;
			fp->_w = 0;
			fp->_lbfsize = 0;
		}
		fp->_flags |= __SRD;
	}

	c = (unsigned char)c;

	/*
	 * If we are in the middle of ungetc'ing, just continue.
	 */
	if (HASUB(fp)) {
		if (fp->_r >= fp->_ub._size && __submore(fp))
			return EOF;
		*--fp->_p = c;
		fp->_r++;
		fp->_flags &= ~__SEOF;  /* Clear EOF on successful ungetc */
		return c;
	}

	/*
	 * If we can handle this by simply backing up, do so,
	 * but never replace a different character.
	 */
	if (fp->_bf._base != NULL && fp->_p > fp->_bf._base &&
	    fp->_p[-1] == c) {
		fp->_p--;
		fp->_r++;
		fp->_flags &= ~__SEOF;  /* Clear EOF on successful ungetc */
		return c;
	}

	/*
	 * Create an ungetc buffer.
	 * Initially, we will use the `reserve' buffer.
	 */
	fp->_ur = fp->_r;
	fp->_up = fp->_p;
	fp->_ub._base = fp->_ubuf;
	fp->_ub._size = sizeof(fp->_ubuf);
	fp->_ubuf[sizeof(fp->_ubuf) - 1] = c;
	fp->_p = &fp->_ubuf[sizeof(fp->_ubuf) - 1];
	fp->_r = 1;
	fp->_flags &= ~__SEOF;  /* Clear EOF on successful ungetc */
	return c;
}

int
__submore(FILE *fp)
{
	int i;
	unsigned char *p;

	if (fp->_ub._base == fp->_ubuf) {
		/*
		 * Get a new buffer (rather than expanding the old one).
		 */
		if ((p = malloc((size_t)BUFSIZ)) == NULL)
			return EOF;
		fp->_ub._base = p;
		fp->_ub._size = BUFSIZ;
		p += BUFSIZ - sizeof(fp->_ubuf);
		for (i = sizeof(fp->_ubuf); --i >= 0;)
			p[i] = fp->_ubuf[i];
		fp->_p = p;
		return 0;
	}
	i = fp->_ub._size;
	p = realloc(fp->_ub._base, (size_t)(i << 1));
	if (p == NULL)
		return EOF;
	/* no overlap (hence can use memcpy) because we doubled the size */
	(void)memcpy((void *)(p + i), (void *)p, (size_t)i);
	fp->_p = p + i;
	fp->_ub._base = p;
	fp->_ub._size = i << 1;
	return 0;
}

