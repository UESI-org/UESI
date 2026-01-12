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
	const char *p_start;
	int w;

	/*
	 * SUSv2 requires a return value of 0 for a count or size of 0.
	 */
	if ((n = count * size) == 0)
		return 0;

	p = p_start = buf;
	
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
		 * Line buffered: write data, flushing on newlines.
		 */
		const char *scan = p;
		const char *end = p + n;
		
		while (scan < end) {
			const char *nl = memchr(scan, '\n', end - scan);
			size_t chunk_len;
			
			if (nl != NULL)
				chunk_len = nl - scan + 1;  /* include the newline */
			else
				chunk_len = end - scan;
			
			/* Write this chunk character by character through buffer */
			while (chunk_len > 0) {
				if (fp->_bf._base == NULL)
					__smakebuf(fp);
				
				/* Try to copy to buffer */
				size_t avail = fp->_w;
				size_t to_copy = chunk_len < avail ? chunk_len : avail;
				
				if (to_copy > 0) {
					memcpy(fp->_p, scan, to_copy);
					fp->_p += to_copy;
					fp->_w -= to_copy;
					scan += to_copy;
					chunk_len -= to_copy;
				}
				
				/* Flush if buffer is full or we just wrote a newline */
				if (fp->_w == 0 || (to_copy > 0 && scan[-1] == '\n')) {
					if (__sflush(fp))
						goto err;
				}
			}
			
			/* Move to next chunk if we had a newline */
			if (nl != NULL && scan < end)
				continue;
		}
	} else {
		/*
		 * Fully buffered: fill buffer, flush when full.
		 */
		do {
			if (fp->_bf._base == NULL)
				__smakebuf(fp);
			
			w = fp->_w;
			if (w > 0) {
				/* Space available in buffer */
				if ((size_t)w > n)
					w = n;
				memcpy(fp->_p, p, w);
				fp->_p += w;
				fp->_w -= w;
			} else if (n >= (size_t)fp->_bf._size) {
				/* Data larger than buffer - flush and write directly */
				if (fp->_p > fp->_bf._base && __sflush(fp))
					goto err;
				
				w = fp->_bf._size;
				if ((size_t)w > n)
					w = n;
				
				if (fp->_write != NULL)
					w = (*fp->_write)(fp->_cookie, p, w);
				else
					w = write(fp->_file, p, w);
				
				if (w <= 0)
					goto err;
			} else {
				/* Buffer is full, flush it */
				if (__sflush(fp))
					goto err;
				continue;
			}
			
			p += w;
			n -= w;
		} while (n > 0);
	}
	
	return count;

err:
	fp->_flags |= __SERR;
	return (size_t)(p - p_start) / size;
}