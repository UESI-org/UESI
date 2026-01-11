#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "stdio_internal.h"

extern int __srefill(FILE *);
extern int __sflush(FILE *);

size_t
fread(void *buf, size_t size, size_t count, FILE *fp)
{
	size_t resid;
	char *p;
	int r;
	size_t total;

	/*
	 * The ANSI standard requires a return value of 0 for a count
	 * or a size of 0.  Peculiarly, it imposes no such requirements
	 * on fwrite; it only requires fwrite to return 0 on error.
	 */
	if ((resid = count * size) == 0)
		return 0;

	total = resid;
	p = buf;

	while (resid > (size_t)(r = fp->_r)) {
		(void)memcpy((void *)p, (void *)fp->_p, (size_t)r);
		fp->_p += r;
		fp->_r -= r;
		p += r;
		resid -= r;
		if (__srefill(fp)) {
			/* no more input: return partial result */
			return (total - resid) / size;
		}
	}
	(void)memcpy((void *)p, (void *)fp->_p, resid);
	fp->_r -= resid;
	fp->_p += resid;
	return count;
}

size_t
fwrite(const void *buf, size_t size, size_t count, FILE *fp)
{
	size_t n;
	const char *p;
	const char *p_start;  /* Track start for return calculation */
	int w;

	/*
	 * SUSv2 requires a return value of 0 for a count or size of 0.
	 */
	if ((n = count * size) == 0)
		return 0;

	p = p_start = buf;
	
	/*
	 * ANSI and SUSv2 require that we will not modify the buffer
	 * if size or count are 0.  Since n == 0 in this case, we
	 * know that we will not do any writes.  Thus, we can skip
	 * the fflush and the main loop.
	 */
	if (fp->_flags & __SNBF) {
		/*
		 * Unbuffered: write directly.
		 */
		if (fp->_write != NULL)
			w = (*fp->_write)(fp->_cookie, p, n);
		else
			w = write(fp->_file, p, n);
		return w == (int)n ? count : (size_t)w / size;
	}
	if (fp->_flags & __SLBF) {
		/*
		 * Line buffered: like fully buffered, but we
		 * must check for newlines.
		 */
		while (n > 0) {
			size_t i = 0;
			/* Find next newline or end of data */
			while (i < n && p[i] != '\n')
				i++;
			if (i < n)
				i++;	/* include the newline */
			
			/* Write this chunk */
			while (i > 0) {
				if (fp->_bf._base == NULL)
					__smakebuf(fp);
				if (--fp->_w < 0) {
					if (__sflush(fp))
						goto err;
					fp->_w = fp->_bf._size - 1;
				}
				*fp->_p++ = *p++;
				i--;
				n--;
			}
			/* Flush if we wrote a newline */
			if (n == 0 || p[-1] == '\n') {
				if (__sflush(fp))
					goto err;
			}
		}
	} else {
		/*
		 * Fully buffered: fill partially full buffer, if any,
		 * then flush.  Afterwards, write the data using the
		 * write function directly.
		 */
		do {
			if (fp->_bf._base == NULL)
				__smakebuf(fp);
			
			w = fp->_bf._size;
			if (fp->_p > fp->_bf._base && (w -= fp->_p - fp->_bf._base) > 0) {
				if ((size_t)w > n)
					w = n;
				(void)memcpy((void *)fp->_p, (void *)p, w);
				fp->_p += w;
				fp->_w -= w;
			} else if (n >= (size_t)(w = fp->_bf._size)) {
				/* write directly */
				if (fp->_p > fp->_bf._base && __sflush(fp))
					goto err;
				if (fp->_write != NULL)
					w = (*fp->_write)(fp->_cookie, p, w);
				else
					w = write(fp->_file, p, w);
				if (w <= 0)
					goto err;
			} else {
				/* fill buffer */
				w = n;
				(void)memcpy((void *)fp->_p, (void *)p, w);
				fp->_p += w;
				fp->_w -= w;
			}
			p += w;
			n -= w;
		} while (n > 0);
	}
	return count;

err:
	return (size_t)(p - p_start) / size;
}