#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include "stdio_internal.h"

#define ALT		   0x001		/* alternate form */
#define LADJUST		0x004		/* left adjustment */
#define LONGDBL		0x008		/* long double */
#define LONGINT		0x010		/* long integer */
#define LLONGINT	0x020		/* long long integer */
#define SHORTINT	0x040		/* short integer */
#define ZEROPAD		0x080		/* zero (as opposed to blank) pad */
#define FPT		    0x100		/* floating point type */
#define SIZEINT		0x200		/* size_t */
#define PTRINT		0x400		/* ptrdiff_t */

size_t len;

static char *__ultoa(char *buf_end, unsigned long val,
    int base, int uppercase, size_t *len);
static char *__ulltoa(char *buf_end, unsigned long long val,
    int base, int uppercase, size_t *len);

int
vfprintf(FILE *fp, const char *fmt, va_list ap)
{
	const char *p;
	char *str;
	int ch, n, flags, width, prec;
	char sign, *cp, buf[512];
	unsigned long ul;
	unsigned long long ull;
	int base;
	int ret = 0;
	char padc;

	for (p = fmt; (ch = *p++) != '\0';) {
		if (ch != '%') {
			if (fputc(ch, fp) == EOF)
				return -1;
			ret++;
			continue;
		}

		/* Process flags */
		flags = 0;
		sign = '\0';
rflag:
		ch = *p++;
		switch (ch) {
		case ' ':
			if (!sign)
				sign = ' ';
			goto rflag;
		case '#':
			flags |= ALT;
			goto rflag;
		case '-':
			flags |= LADJUST;
			goto rflag;
		case '+':
			sign = '+';
			goto rflag;
		case '0':
			flags |= ZEROPAD;
			goto rflag;
		}

		/* Width */
		width = 0;
		if (ch == '*') {
			width = va_arg(ap, int);
			if (width < 0) {
				width = -width;
				flags |= LADJUST;
			}
			ch = *p++;
		} else {
			while (isdigit(ch)) {
				width = width * 10 + (ch - '0');
				ch = *p++;
			}
		}

		/* Precision */
		prec = -1;
		if (ch == '.') {
			ch = *p++;
			if (ch == '*') {
				prec = va_arg(ap, int);
				ch = *p++;
			} else {
				prec = 0;
				while (isdigit(ch)) {
					prec = prec * 10 + (ch - '0');
					ch = *p++;
				}
			}
		}

		/* Length modifiers */
		switch (ch) {
		case 'h':
			if (*p == 'h') {
				p++;
				/* char (promoted to int) - treat same as short */
			}
			flags |= SHORTINT;
			ch = *p++;
			break;
		case 'l':
			if (*p == 'l') {
				p++;
				flags |= LLONGINT;
			} else
				flags |= LONGINT;
			ch = *p++;
			break;
		case 'z':
			flags |= SIZEINT;
			ch = *p++;
			break;
		case 't':
			flags |= PTRINT;
			ch = *p++;
			break;
		case 'L':
			flags |= LONGDBL;
			ch = *p++;
			break;
		}

		/* Conversion specifier */
		switch (ch) {
		case 'd':
		case 'i':
			if (flags & LLONGINT) {
				long long ll = va_arg(ap, long long);
				if (ll < 0) {
					sign = '-';
					ull = -ll;
				} else
					ull = ll;
			} else if (flags & SIZEINT) {
				ssize_t ss = va_arg(ap, ssize_t);
				if (ss < 0) {
					sign = '-';
					ul = -ss;
				} else
					ul = ss;
			} else if (flags & PTRINT) {
				ptrdiff_t pd = va_arg(ap, ptrdiff_t);
				if (pd < 0) {
					sign = '-';
					ul = -pd;
				} else
					ul = pd;
			} else {
				long l = (flags & LONGINT) ? va_arg(ap, long) : va_arg(ap, int);
				if (flags & SHORTINT)
					l = (short)l;
				if (l < 0) {
					sign = '-';
					ul = -l;
				} else
					ul = l;
			}
			base = 10;
			goto number;

		case 'o':
			base = 8;
			goto unsignednumber;
		case 'u':
			base = 10;
			goto unsignednumber;
		case 'x':
		case 'X':
			base = 16;
unsignednumber:
			if (flags & LLONGINT)
				ull = va_arg(ap, unsigned long long);
			else if (flags & SIZEINT)
				ul = va_arg(ap, size_t);
			else if (flags & PTRINT)
				ul = va_arg(ap, ptrdiff_t);
			else {
				ul = (flags & LONGINT) ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
				if (flags & SHORTINT)
					ul = (unsigned short)ul;
			}
number:
			if (flags & LLONGINT)
				cp = __ulltoa(buf + sizeof(buf), ull, base, ch == 'X', &len);
			else
				cp = __ultoa(buf + sizeof(buf), ul, base, ch == 'X', &len);
			
			if (flags & ALT && base == 8 && *cp != '0')
				*--cp = '0';

			n = (buf + sizeof(buf)) - cp;
			
			/* Add sign */
			if (sign) {
				*--cp = sign;
				n++;
			}

			/* Add 0x/0X prefix for hex */
			if (flags & ALT && base == 16 && (ull || ul)) {
				*--cp = ch;
				*--cp = '0';
				n += 2;
			}

			/* Padding */
			padc = (flags & ZEROPAD) ? '0' : ' ';
			if (!(flags & LADJUST)) {
				while (n < width) {
					if (fputc(padc, fp) == EOF)
						return -1;
					ret++;
					width--;
				}
			}

			/* Output the number */
			while (n--) {
				if (fputc(*cp++, fp) == EOF)
					return -1;
				ret++;
			}

			/* Right padding */
			while (width-- > 0) {
				if (fputc(' ', fp) == EOF)
					return -1;
				ret++;
			}
			break;

		case 'c':
			buf[0] = va_arg(ap, int);
			cp = buf;
			n = 1;
			goto strout;

		case 's':
			str = va_arg(ap, char *);
			if (str == NULL)
				str = "(null)";
			if (prec >= 0) {
				cp = memchr(str, '\0', prec);
				if (cp != NULL)
					n = cp - str;
				else
					n = prec;
			} else
				n = strlen(str);
			cp = str;
strout:
			if (!(flags & LADJUST)) {
				while (n < width) {
					if (fputc(' ', fp) == EOF)
						return -1;
					ret++;
					width--;
				}
			}
			while (n--) {
				if (fputc(*cp++, fp) == EOF)
					return -1;
				ret++;
			}
			while (width-- > 0) {
				if (fputc(' ', fp) == EOF)
					return -1;
				ret++;
			}
			break;

		case 'p':
			ul = (unsigned long)va_arg(ap, void *);
			cp = __ultoa(buf + sizeof(buf), ul, 16, 0, &len);
			*--cp = 'x';
			*--cp = '0';
			n = (buf + sizeof(buf)) - cp;
			goto strout;

		case 'n':
			if (flags & LLONGINT)
				*va_arg(ap, long long *) = ret;
			else if (flags & LONGINT)
				*va_arg(ap, long *) = ret;
			else if (flags & SHORTINT)
				*va_arg(ap, short *) = ret;
			else
				*va_arg(ap, int *) = ret;
			break;

		case '%':
			if (fputc('%', fp) == EOF)
				return -1;
			ret++;
			break;

		default:
			if (fputc('%', fp) == EOF)
				return -1;
			if (fputc(ch, fp) == EOF)
				return -1;
			ret += 2;
			break;
		}
	}
	return ret;
}

int
fprintf(FILE *fp, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = vfprintf(fp, fmt, ap);
	va_end(ap);
	return ret;
}

int
printf(const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = vfprintf(stdout, fmt, ap);
	va_end(ap);
	return ret;
}

int
vprintf(const char *fmt, va_list ap)
{
	return vfprintf(stdout, fmt, ap);
}

int
sprintf(char *str, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = vsprintf(str, fmt, ap);
	va_end(ap);
	return ret;
}

int
vsprintf(char *str, const char *fmt, va_list ap)
{
	FILE f;
	int ret;

	/* Create a fake FILE for string output */
	memset(&f, 0, sizeof(f));
	f._flags = __SWR | __SSTR;
	f._bf._base = f._p = (unsigned char *)str;
	f._bf._size = f._w = INT_MAX;
	
	ret = vfprintf(&f, fmt, ap);
	*f._p = '\0';
	return ret;
}

int
snprintf(char *str, size_t size, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = vsnprintf(str, size, fmt, ap);
	va_end(ap);
	return ret;
}

int
vsnprintf(char *str, size_t size, const char *fmt, va_list ap)
{
	FILE f;
	int ret;
	char dummy;

	/* Handle size == 0 case - still need to count chars */
	if (size == 0) {
		/* Use dummy buffer to count */
		memset(&f, 0, sizeof(f));
		f._flags = __SWR | __SSTR;
		f._bf._base = f._p = (unsigned char *)&dummy;
		f._bf._size = f._w = 0;
		return vfprintf(&f, fmt, ap);
	}

	/* Create a fake FILE for string output */
	memset(&f, 0, sizeof(f));
	f._flags = __SWR | __SSTR;
	f._bf._base = f._p = (unsigned char *)str;
	f._bf._size = f._w = size - 1;
	
	ret = vfprintf(&f, fmt, ap);
	*f._p = '\0';

	return ret;
}

#if __POSIX_VISIBLE >= 200809
int
dprintf(int fd, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = vdprintf(fd, fmt, ap);
	va_end(ap);
	return ret;
}

int
vdprintf(int fd, const char *fmt, va_list ap)
{
	FILE *fp;
	int ret;

	if ((fp = fdopen(fd, "w")) == NULL)
		return -1;
	ret = vfprintf(fp, fmt, ap);
	fflush(fp);
	return ret;
}
#endif

static char *
__ultoa(char *buf_end, unsigned long val, int base, int uppercase, size_t *len)
{
	char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
	char *s = buf_end;
	size_t count = 0;
	
	*--s = '\0';
	count++;
	
	if (val == 0) {
		*--s = '0';
		count++;
		if (len) *len = count;
		return s;
	}
	
	while (val) {
		*--s = digits[val % base];
		val /= base;
		count++;
	}
	
	if (len) *len = count;
	return s;
}

static char *
__ulltoa(char *buf_end, unsigned long long val, int base, int uppercase, size_t *len)
{
	char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
	char *s = buf_end;
	size_t count = 0;
	
	*--s = '\0';
	count++;
	
	if (val == 0) {
		*--s = '0';
		count++;
		if (len) *len = count;
		return s;
	}
	
	while (val) {
		*--s = digits[val % base];
		val /= base;
		count++;
	}
	
	if (len) *len = count;
	return s;
}