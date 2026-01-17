#ifndef _STDIO_H_
#define _STDIO_H_

#include <sys/cdefs.h>
#include <sys/stdarg.h>
#include <sys/_null.h>
#include <sys/_types.h>

#ifndef _SIZE_T_DEFINED_
#define _SIZE_T_DEFINED_
typedef __size_t size_t;
#endif

#ifndef _SSIZE_T_DEFINED_
#define _SSIZE_T_DEFINED_
typedef __ssize_t ssize_t;
#endif

#ifndef _OFF_T_DEFINED_
#define _OFF_T_DEFINED_
typedef __off_t off_t;
#endif

#ifndef _FPOS_T_DEFINED_
#define _FPOS_T_DEFINED_
typedef __fpos_t fpos_t;
#endif

#if __POSIX_VISIBLE >= 200809
#ifndef _VA_LIST_DEFINED_
#define _VA_LIST_DEFINED_
typedef __gnuc_va_list va_list;
#endif
#endif

#define _FSTDIO /* Define stdio internals */

/*
 * stdio state variables.
 *
 * The following always hold:
 *
 *	if (_flags&(__SLBF|__SWR)) == (__SLBF|__SWR),
 *		_lbfsize is -_bf._size, else _lbfsize is 0
 *	if _flags&__SRD, _w is 0
 *	if _flags&__SWR, _r is 0
 *
 * This ensures that the getc and putc macros (or inline functions) never
 * try to write or read from a file that is in `read' or `write' mode.
 * (Moreover, they can, and do, automatically switch from read mode to
 * write mode, and back, on "r+" and "w+" files.)
 *
 * _lbfsize is used only to make the inline line-buffered output stream
 * code as compact as possible.
 *
 * WARNING: Do not change the order of the fields; this is known by <stdio.h>
 * internals.
 */
struct __sbuf {
	unsigned char *_base;
	int _size;
};

/*
 * stdio buffers.
 * This is a public structure that may need to be binary compatible.
 */
typedef struct __sFILE {
	unsigned char *_p;	/* current position in (some) buffer */
	int _r;			/* read space left for getc() */
	int _w;			/* write space left for putc() */
	short _flags;		/* flags, below; this FILE is free if 0 */
	short _file;		/* fileno, if Unix descriptor, else -1 */
	struct __sbuf _bf;	/* the buffer (at least 1 byte, if !NULL) */
	int _lbfsize;		/* 0 or -_bf._size, for inline putc */

	/* operations */
	void *_cookie;		/* cookie passed to io functions */
	int (*_close)(void *);
	int (*_read)(void *, char *, int);
	fpos_t (*_seek)(void *, fpos_t, int);
	int (*_write)(void *, const char *, int);

	/* separate buffer for long sequences of ungetc() */
	struct __sbuf _ub;	/* ungetc buffer */
	unsigned char *_up;	/* saved _p when _p is doing ungetc data */
	int _ur;		/* saved _r when _r is counting ungetc data */

	/* tricks to meet minimum requirements even when malloc() fails */
	unsigned char _ubuf[3];	/* guarantee an ungetc() buffer */
	unsigned char _nbuf[1];	/* guarantee a getc() buffer */

	/* separate buffer for fgetln() when line crosses buffer boundary */
	struct __sbuf _lb;	/* buffer for fgetln() */

	/* Unix stdio files get aligned to block boundaries on fseek() */
	int _blksize;		/* stat.st_blksize (may be != _bf._size) */
	fpos_t _offset;		/* current lseek offset */
} FILE;

__BEGIN_DECLS

extern FILE __sF[];

__END_DECLS

#define __SLBF	0x0001		/* line buffered */
#define __SNBF	0x0002		/* unbuffered */
#define __SRD	0x0004		/* OK to read */
#define __SWR	0x0008		/* OK to write */
	/* RD and WR are never simultaneously asserted */
#define __SRW	0x0010		/* open for reading & writing */
#define __SEOF	0x0020		/* found EOF */
#define __SERR	0x0040		/* found error */
#define __SMBF	0x0080		/* _buf is from malloc */
#define __SAPP	0x0100		/* fdopen()ed in append mode */
#define __SSTR	0x0200		/* this is an sprintf/snprintf string */
#define __SOPT	0x0400		/* do fseek() optimization */
#define __SNPT	0x0800		/* do not do fseek() optimization */
#define __SOFF	0x1000		/* set iff _offset is in fact correct */
#define __SMOD	0x2000		/* true => fgetln modified _p text */
#define __SALC	0x4000		/* allocate string space dynamically */

/*
 * The following three definitions are for ANSI C, which took them
 * from System V, which brilliantly took internal interface macros and
 * made them official arguments to setvbuf(), without renaming them.
 * Hence, these ugly _IOxxx names are *supposed* to appear in user code.
 *
 * Although numbered as their counterparts above, the implementation
 * does not rely on this.
 */
#define _IOFBF	0		/* setvbuf should set fully buffered */
#define _IOLBF	1		/* setvbuf should set line buffered */
#define _IONBF	2		/* setvbuf should set unbuffered */

#define BUFSIZ	4096	/* size of buffer used by setbuf */

#define EOF	(-1)

/*
 * FOPEN_MAX is a minimum maximum, and is the number of streams that
 * stdio can provide without attempting to allocate further resources
 * (which could fail).  Do not use this for anything.
 */
				/* must be == _POSIX_STREAM_MAX <limits.h> */
#define FOPEN_MAX	20	/* must be <= OPEN_MAX <sys/syslimits.h> */
#define FILENAME_MAX	1024	/* must be <= PATH_MAX <sys/syslimits.h> */

/* System V/ANSI C; this is the wrong way to do this, do *not* use these. */
#if __XPG_VISIBLE
#define P_tmpdir	"/tmp/"
#endif
#define L_tmpnam	1024	/* XXX must be == PATH_MAX */
#define TMP_MAX		308915776

#ifndef SEEK_SET
#define SEEK_SET	0	/* set file offset to offset */
#endif
#ifndef SEEK_CUR
#define SEEK_CUR	1	/* set file offset to current plus offset */
#endif
#ifndef SEEK_END
#define SEEK_END	2	/* set file offset to EOF plus offset */
#endif

#define stdin	(&__sF[0])
#define stdout	(&__sF[1])
#define stderr	(&__sF[2])

__BEGIN_DECLS

/*
 * Functions defined in ANSI C standard.
 */
void	 clearerr(FILE *);
int	 fclose(FILE *);
int	 feof(FILE *);
int	 ferror(FILE *);
int	 fflush(FILE *);
int	 fgetc(FILE *);
int	 fgetpos(FILE * __restrict, fpos_t * __restrict);
char	*fgets(char * __restrict, int, FILE * __restrict);
FILE	*fopen(const char * __restrict, const char * __restrict);
int	 fprintf(FILE * __restrict, const char * __restrict, ...)
		__attribute__((__format__ (printf, 2, 3)));
int	 fputc(int, FILE *);
int	 fputs(const char * __restrict, FILE * __restrict);
size_t	 fread(void * __restrict, size_t, size_t, FILE * __restrict);
FILE	*freopen(const char * __restrict, const char * __restrict,
	    FILE * __restrict);
int	 fscanf(FILE * __restrict, const char * __restrict, ...)
		__attribute__((__format__ (scanf, 2, 3)));
int	 fseek(FILE *, long, int);
int	 fsetpos(FILE *, const fpos_t *);
long	 ftell(FILE *);
size_t	 fwrite(const void * __restrict, size_t, size_t, FILE * __restrict);
int	 getc(FILE *);
int	 getchar(void);
char	*gets(char *)
		__attribute__((__deprecated__("unsafe; use fgets")));
void	 perror(const char *);
int	 printf(const char * __restrict, ...)
		__attribute__((__format__ (printf, 1, 2)));
int	 putc(int, FILE *);
int	 putchar(int);
int	 puts(const char *);
int	 remove(const char *);
int	 rename(const char *, const char *);
void	 rewind(FILE *);
int	 scanf(const char * __restrict, ...)
		__attribute__((__format__ (scanf, 1, 2)));
void	 setbuf(FILE * __restrict, char * __restrict);
int	 setvbuf(FILE * __restrict, char * __restrict, int, size_t);
int	 sprintf(char * __restrict, const char * __restrict, ...)
		__attribute__((__format__ (printf, 2, 3)));
int	 sscanf(const char * __restrict, const char * __restrict, ...)
		__attribute__((__format__ (scanf, 2, 3)));
FILE	*tmpfile(void);
char	*tmpnam(char *);
int	 ungetc(int, FILE *);
int	 vfprintf(FILE * __restrict, const char * __restrict, __gnuc_va_list)
		__attribute__((__format__ (printf, 2, 0)));
int	 vprintf(const char * __restrict, __gnuc_va_list)
		__attribute__((__format__ (printf, 1, 0)));
int	 vsprintf(char * __restrict, const char * __restrict, __gnuc_va_list)
		__attribute__((__format__ (printf, 2, 0)));

#if __ISO_C_VISIBLE >= 1999 || __BSD_VISIBLE
int	 snprintf(char * __restrict, size_t, const char * __restrict, ...)
		__attribute__((__format__ (printf, 3, 4)));
int	 vsnprintf(char * __restrict, size_t, const char * __restrict,
	    __gnuc_va_list)
		__attribute__((__format__ (printf, 3, 0)));
int	 vfscanf(FILE * __restrict, const char * __restrict, __gnuc_va_list)
		__attribute__((__format__ (scanf, 2, 0)));
int	 vscanf(const char * __restrict, __gnuc_va_list)
		__attribute__((__format__ (scanf, 1, 0)));
int	 vsscanf(const char * __restrict, const char * __restrict,
	    __gnuc_va_list)
		__attribute__((__format__ (scanf, 2, 0)));
#endif /* __ISO_C_VISIBLE >= 1999 || __BSD_VISIBLE */

#if __POSIX_VISIBLE
int	 fileno(FILE *);
#endif

#if __POSIX_VISIBLE >= 199506
void	 flockfile(FILE *);
int	 ftrylockfile(FILE *);
void	 funlockfile(FILE *);

/*
 * These are normally used through macros as defined below, but POSIX
 * requires functions as well.
 */
int	 getc_unlocked(FILE *);
int	 getchar_unlocked(void);
int	 putc_unlocked(int, FILE *);
int	 putchar_unlocked(int);
#endif /* __POSIX_VISIBLE >= 199506 */

#if __POSIX_VISIBLE >= 200112 || __XPG_VISIBLE >= 500
int	 fseeko(FILE *, off_t, int);
off_t	 ftello(FILE *);
#endif

#if __POSIX_VISIBLE >= 200809
FILE	*fmemopen(void * __restrict, size_t, const char * __restrict);
ssize_t	 getdelim(char ** __restrict, size_t * __restrict, int,
	    FILE * __restrict);
ssize_t	 getline(char ** __restrict, size_t * __restrict,
	    FILE * __restrict);
FILE	*open_memstream(char **, size_t *);
int	 renameat(int, const char *, int, const char *);
int	 vdprintf(int, const char * __restrict, __gnuc_va_list)
		__attribute__((__format__ (printf, 2, 0)));
int	 dprintf(int, const char * __restrict, ...)
		__attribute__((__format__ (printf, 2, 3)));
#endif /* __POSIX_VISIBLE >= 200809 */

/*
 * Functions defined in all versions of POSIX 1003.1.
 */
#if __BSD_VISIBLE || __POSIX_VISIBLE || __XPG_VISIBLE
#define L_ctermid	1024	/* size for ctermid(); PATH_MAX */
char	*ctermid(char *);
FILE	*fdopen(int, const char *);
#endif
#if (__BSD_VISIBLE || __POSIX_VISIBLE >= 199209)
int	 pclose(FILE *);
FILE	*popen(const char *, const char *);
#endif

#if __BSD_VISIBLE || (__XPG_VISIBLE && __XPG_VISIBLE < 500)
int	 getw(FILE *);
int	 putw(int, FILE *);
#endif /* __BSD_VISIBLE || (__XPG_VISIBLE && __XPG_VISIBLE < 500) */

#if __XPG_VISIBLE >= 420
char	*tempnam(const char *, const char *);
#endif

/*
 * Routines that are purely local.
 */
#if __BSD_VISIBLE
char	*fgetln(FILE * __restrict, size_t * __restrict);
int	 fpurge(FILE *);
int	 getw(FILE *);
int	 putw(int, FILE *);
void	 setbuffer(FILE *, char *, int);
int	 setlinebuf(FILE *);
int	 asprintf(char ** __restrict, const char * __restrict, ...)
		__attribute__((__format__ (printf, 2, 3)));
int	 vasprintf(char ** __restrict, const char * __restrict, __gnuc_va_list)
		__attribute__((__format__ (printf, 2, 0)));
#endif

/*
 * Stdio function-access interface.
 */
#if __BSD_VISIBLE
FILE	*funopen(const void *,
		int (*)(void *, char *, int),
		int (*)(void *, const char *, int),
		fpos_t (*)(void *, fpos_t, int),
		int (*)(void *));
#define	fropen(cookie, fn) funopen(cookie, fn, NULL, NULL, NULL)
#define	fwopen(cookie, fn) funopen(cookie, NULL, fn, NULL, NULL)
#endif /* __BSD_VISIBLE */

/*
 * Functions internal to the implementation.
 */
int	__srget(FILE *);
int	__swbuf(int, FILE *);

/*
 * The __sfoo macros are here so that we can
 * define function versions in the C library.
 */
#define __sgetc(p) (--(p)->_r < 0 ? __srget(p) : (int)(*(p)->_p++))

static __inline int __sputc(int _c, FILE *_p) {
	if (--_p->_w >= 0 || (_p->_w >= _p->_lbfsize && (char)_c != '\n'))
		return (*_p->_p++ = _c);
	else
		return (__swbuf(_c, _p));
}

#define __sfeof(p)	(((p)->_flags & __SEOF) != 0)
#define __sferror(p)	(((p)->_flags & __SERR) != 0)
#define __sclearerr(p)	((void)((p)->_flags &= ~(__SERR|__SEOF)))
#define __sfileno(p)	((p)->_file)

#ifndef __cplusplus
#define	feof(p)		__sfeof(p)
#define	ferror(p)	__sferror(p)
#define	clearerr(p)	__sclearerr(p)

#if __POSIX_VISIBLE
#define	fileno(p)	__sfileno(p)
#endif

#ifndef _POSIX_THREADS
#define	getc(fp)	__sgetc(fp)
#define putc(x, fp)	__sputc(x, fp)
#endif /* _POSIX_THREADS */

#define	getchar()	getc(stdin)
#define	putchar(x)	putc(x, stdout)

#if __BSD_VISIBLE
/*
 * See ISO/IEC 9945-1 ANSI/IEEE Std 1003.1 Second Edition 1996-07-12
 * B.8.2.7 for the rationale behind the *_unlocked() macros.
 */
#define	feof_unlocked(p)	__sfeof(p)
#define	ferror_unlocked(p)	__sferror(p)
#define	clearerr_unlocked(p)	__sclearerr(p)
#define	fileno_unlocked(p)	__sfileno(p)
#endif /* __BSD_VISIBLE */

#if __POSIX_VISIBLE >= 199506
#define	getc_unlocked(fp)	__sgetc(fp)
#define	putc_unlocked(x, fp)	__sputc(x, fp)

#define	getchar_unlocked()	getc_unlocked(stdin)
#define	putchar_unlocked(x)	putc_unlocked(x, stdout)
#endif /* __POSIX_VISIBLE >= 199506 */
#endif /* __cplusplus */

__END_DECLS

#endif /* _STDIO_H_ */