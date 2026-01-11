#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "stdio_internal.h"

#undef fputc
#undef putc
#undef putchar
#undef putc_unlocked
#undef putchar_unlocked

int
__swbuf(int c, FILE *fp)
{
	int n;

	fp->_w = fp->_lbfsize;
	if (fp->_flags & __SNBF) {
		unsigned char ch = c;
		if (fp->_write != NULL)
			n = (*fp->_write)(fp->_cookie, (char *)&ch, 1);
		else
			n = write(fp->_file, &ch, 1);
		if (n != 1) {
			fp->_flags |= __SERR;
			return EOF;
		}
		return c;
	}
	if (fp->_bf._base == NULL)
		__smakebuf(fp);
	
	if (fp->_flags & __SLBF) {
		*fp->_p++ = c;
		if (c == '\n' || fp->_p >= fp->_bf._base + fp->_bf._size) {
			if (__sflush(fp))
				return EOF;
			fp->_w = -(fp->_bf._size);
		} else
			fp->_w--;
		return c;
	}

	*fp->_p++ = c;
	if (--fp->_w < 0 && __sflush(fp))
		return EOF;
	return c;
}

int
fputc(int c, FILE *fp)
{
	return __sputc(c, fp);
}

int
putc(int c, FILE *fp)
{
	return __sputc(c, fp);
}

int
putchar(int c)
{
	return putc(c, stdout);
}

int
putc_unlocked(int c, FILE *fp)
{
	return __sputc(c, fp);
}

int
putchar_unlocked(int c)
{
	return __sputc(c, stdout);
}

int
fputs(const char *s, FILE *fp)
{
	size_t len;
	struct __suio uio;
	struct __siov iov;

	if (s == NULL || fp == NULL) {
		errno = EINVAL;
		return EOF;
	}

	len = strlen(s);
	if (len == 0)
		return 0;

	iov.iov_base = (void *)s;
	iov.iov_len = uio.uio_resid = len;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;

	if (__sfvwrite(fp, &uio) != 0)
		return EOF;
	return len;
}

int
puts(const char *s)
{
	size_t c;
	struct __suio uio;
	struct __siov iov[2];
	
	if (s == NULL) {
		errno = EINVAL;
		return EOF;
	}
	
	iov[0].iov_base = (void *)s;
	iov[0].iov_len = c = strlen(s);
	iov[1].iov_base = "\n";
	iov[1].iov_len = 1;
	uio.uio_resid = c + 1;
	uio.uio_iov = &iov[0];
	uio.uio_iovcnt = 2;
	if (__sfvwrite(stdout, &uio))
		return EOF;
	return '\n';
}

int
__sfvwrite(FILE *fp, struct __suio *uio)
{
	size_t len;
	char *p;
	struct __siov *iov;
	int w, s;
	char *nl;
	int nlknown, nldist;

	if ((len = uio->uio_resid) == 0)
		return 0;

	if ((fp->_flags & __SWR) == 0) {
		if (fp->_flags & __SRD) {
			if (HASUB(fp))
				FREEUB(fp);
			fp->_flags &= ~(__SRD|__SEOF);
			fp->_r = 0;
			fp->_p = fp->_bf._base;
		}
		fp->_flags |= __SWR;
		fp->_w = fp->_flags & (__SLBF|__SNBF) ? 0 : fp->_bf._size;
	}

	if (fp->_flags & __SNBF) {
		do {
			iov = uio->uio_iov;
			p = iov->iov_base;
			len = iov->iov_len;
			uio->uio_resid -= len;
			
			if (fp->_write != NULL)
				w = (*fp->_write)(fp->_cookie, p, len);
			else
				w = write(fp->_file, p, len);
			if (w != (int)len)
				goto err;
		} while (--uio->uio_iovcnt > 0 && uio++);
	} else if (fp->_flags & __SLBF) {
		nlknown = 0;
		nldist = 0;
		do {
			iov = uio->uio_iov;
			p = iov->iov_base;
			len = iov->iov_len;
			uio->uio_iov++;
			uio->uio_iovcnt--;
			uio->uio_resid -= len;

			if (!nlknown) {
				nl = memchr(p, '\n', len);
				nldist = nl ? nl + 1 - p : len + 1;
				nlknown = 1;
			}
			
			s = fp->_bf._size ? fp->_bf._size : 1;
			while (len >= (size_t)s) {
				if ((size_t)(w = fp->_w) >= len || (w >= nldist && nldist <= (int)len)) {
					memcpy(fp->_p, p, w);
					fp->_w = 0;
					fp->_p += w;
					if (__sflush(fp))
						goto err;
				} else if (fp->_p > fp->_bf._base && (size_t)(w = fp->_p - fp->_bf._base) >= (size_t)nldist) {
					memcpy(fp->_p, p, nldist);
					fp->_p += nldist;
					if (__sflush(fp))
						goto err;
				} else {
					w = len >= (size_t)s ? s : len;
					memcpy(fp->_p, p, w);
					fp->_w -= w;
					fp->_p += w;
				}
				p += w;
				len -= w;
			}
			if (len) {
				memcpy(fp->_p, p, len);
				fp->_w -= len;
				fp->_p += len;
			}
			nlknown = 0;
		} while (uio->uio_iovcnt > 0);
	} else {
		do {
			iov = uio->uio_iov;
			p = iov->iov_base;
			len = iov->iov_len;
			uio->uio_iov++;
			uio->uio_iovcnt--;
			uio->uio_resid -= len;
			
			while ((size_t)(w = fp->_w) < len) {
				if (w > 0) {
					memcpy(fp->_p, p, w);
					fp->_p += w;
					p += w;
					len -= w;
				}
				if (__sflush(fp))
					goto err;
			}
			if (len > 0) {
				memcpy(fp->_p, p, len);
				fp->_w -= len;
				fp->_p += len;
			}
		} while (uio->uio_iovcnt > 0);
	}
	return 0;

err:
	fp->_flags |= __SERR;
	return EOF;
}