#ifndef _STDIO_INTERNAL_H_
#define _STDIO_INTERNAL_H_

#include <stdio.h>
#include <sys/types.h>

/* Internal helper macros */
#define HASUB(fp) ((fp)->_ub._base != NULL)
#define FREEUB(fp) do { \
	if ((fp)->_ub._size > sizeof((fp)->_ubuf)) \
		free((fp)->_ub._base); \
	(fp)->_ub._base = NULL; \
} while (0)

/* Internal functions */
FILE *__sfp(void);
int __sflags(const char *mode, int *optr);
int __sflush(FILE *fp);
int __sflush_all(void);
int __srefill(FILE *fp);
void __smakebuf(FILE *fp);
int __swhatbuf(FILE *fp, size_t *bufsize, int *couldbetty);
void __scleanup(FILE *fp);
int __swbuf(int c, FILE *fp);
int __srget(FILE *fp);
fpos_t __sseek(void *cookie, fpos_t offset, int whence);
int __sread(void *cookie, char *buf, int n);
int __swrite(void *cookie, const char *buf, int n);
int __sclose(void *cookie);
int __submore(FILE *fp);

struct __siov {
	void *iov_base;
	size_t iov_len;
};

struct __suio {
	struct __siov *uio_iov;
	int uio_iovcnt;
	size_t uio_resid;
};

int __sfvwrite(FILE *fp, struct __suio *uio);

#endif
