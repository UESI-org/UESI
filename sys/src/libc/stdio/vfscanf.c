#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "stdio_internal.h"

#define CT_INT		0
#define SUPPRESS	0x001	/* suppress assignment */
#define SHORTINT	0x002	/* short integer */
#define LONGINT		0x004	/* long integer */
#define LLONGINT	0x008	/* long long integer */
#define SIZEINT		0x010	/* size_t */

static const char *
__sccl(char *tab, const char *fmt)
{
	int c, n, v;

	/* first `clear' the whole table */
	c = *fmt++;		/* first char hat => negated scanset */
	if (c == '^') {
		v = 1;		/* default => accept */
		c = *fmt++;	/* get new first char */
	} else
		v = 0;		/* default => reject */

	/* should probably use memset here */
	for (n = 0; n < 256; n++)
		tab[n] = v;

	if (c == 0)
		return fmt - 1;	/* format ended too soon */

	/*
	 * Now set the entries corresponding to the actual scanset to the
	 * opposite of the above. The first character may be ']' (or '-')
	 * without being special; the last character may be '-'.
	 */
	v = 1 - v;
	
	/* Handle ] as first character: []...] or [^]...] */
	if (c == ']') {
		tab[c] = v;
		c = *fmt++;
		if (c == 0)
			return fmt - 1;
	}
	
	for (;;) {
		tab[c] = v;		/* take character c */
doswitch:
		n = *fmt++;		/* and examine the next */
		switch (n) {
		case 0:			/* format ended too soon */
			return fmt - 1;

		case '-':
			/*
			 * A scanset of the form [01+-] is defined as `the digit 0, the
			 * digit 1, the character +, the character -', but the effect of a
			 * scanset such as [a-zA-Z0-9] is implementation defined.  The V7
			 * Unix scanf treats `a-z' as `the letters a through z', but treats
			 * `a-a' as `the letter a, the character -, and the letter a'.
			 *
			 * For compatibility, the `-' is not considered to define a range if
			 * the character following it is either a close bracket (required by
			 * ANSI) or is not numerically greater than the character we just
			 * stored in the table (c).
			 */
			n = *fmt;
			if (n == ']' || n < c || n == 0) {
				c = '-';
				break;	/* resume the for(;;) */
			}
			fmt++;
			/* fill in the range */
			do {
				tab[++c] = v;
			} while (c < n);
			c = n;
			/*
			 * Alas, the V7 Unix scanf also treats formats such as [a-c-e] as
			 * `the letters a through e'. This too is permitted by the
			 * standard....
			 */
			goto doswitch;

		case ']':		/* end of scanset */
			return fmt;

		default:		/* just another character */
			c = n;
			break;
		}
	}
	/* NOTREACHED */
}

int
vfscanf(FILE *fp, const char *fmt, va_list ap)
{
	int c;			/* character from format, or conversion */
	size_t width;		/* field width, or 0 */
	char *p;		/* points into all kinds of strings */
	int n;			/* handy integer */
	int flags;		/* flags as defined above */
	char *p0;		/* saves original value of p when necessary */
	int nassigned;		/* number of fields assigned */
	int nread;		/* number of characters consumed from fp */
	int base;		/* base argument to strtol/strtoul */
	char ccltab[256];	/* character class table for %[...] */
	char buf[128];		/* buffer for numeric conversions */
	unsigned long (*ccfn)(const char *, char **, int);

	nassigned = 0;
	nread = 0;
	for (;;) {
		c = *fmt++;
		if (c == 0)
			return nassigned;
		if (isspace(c)) {
			while ((n = fgetc(fp)) != EOF && isspace(n))
				nread++;
			if (n != EOF)
				ungetc(n, fp);
			continue;
		}
		if (c != '%')
			goto literal;
		width = 0;
		flags = 0;

		/* Get width */
		if (*fmt == '*') {
			flags |= SUPPRESS;
			fmt++;
		}
		while (isdigit(*fmt))
			width = width * 10 + (*fmt++ - '0');

		/* Length modifiers */
		switch (*fmt) {
		case 'h':
			if (fmt[1] == 'h') {
				fmt += 2;
			} else {
				fmt++;
				flags |= SHORTINT;
			}
			break;
		case 'l':
			if (fmt[1] == 'l') {
				fmt += 2;
				flags |= LLONGINT;
			} else {
				fmt++;
				flags |= LONGINT;
			}
			break;
		case 'z':
			fmt++;
			flags |= SIZEINT;
			break;
		}

		/* Conversion specifiers */
		c = *fmt++;
		switch (c) {
		case '%':
literal:
			n = fgetc(fp);
			if (n != c) {
				if (n != EOF)
					ungetc(n, fp);
				return nassigned;
			}
			nread++;
			break;

		case 'd':
			c = CT_INT;
			ccfn = (unsigned long (*)(const char *, char **, int))strtol;
			base = 10;
			goto number;

		case 'i':
			c = CT_INT;
			ccfn = (unsigned long (*)(const char *, char **, int))strtol;
			base = 0;
			goto number;

		case 'o':
			c = CT_INT;
			ccfn = strtoul;
			base = 8;
			goto number;

		case 'u':
			c = CT_INT;
			ccfn = strtoul;
			base = 10;
			goto number;

		case 'x':
		case 'X':
			c = CT_INT;
			ccfn = strtoul;
			base = 16;

number:
			/* Collect the number */
			p = buf;
			n = fgetc(fp);
			
			/* Handle sign */
			if (n == '+' || n == '-') {
				*p++ = n;
				n = fgetc(fp);
				nread++;
			}

			/* Collect digits */
			while ((width == 0 || width-- > 0) && n != EOF) {
				if (isdigit(n) || (base == 16 && isxdigit(n))) {
					*p++ = n;
					n = fgetc(fp);
					nread++;
				} else
					break;
			}
			if (n != EOF)
				ungetc(n, fp);
			*p = '\0';

			if (p == buf)
				return nassigned;

			/* Convert */
			if (!(flags & SUPPRESS)) {
				unsigned long res = (*ccfn)(buf, NULL, base);
				if (flags & LLONGINT)
					*va_arg(ap, long long *) = res;
				else if (flags & SIZEINT)
					*va_arg(ap, size_t *) = res;
				else if (flags & LONGINT)
					*va_arg(ap, long *) = res;
				else if (flags & SHORTINT)
					*va_arg(ap, short *) = (short)res;
				else
					*va_arg(ap, int *) = (int)res;
				nassigned++;
			}
			break;

		case 'c':
			if (width == 0)
				width = 1;
			if (flags & SUPPRESS) {
				while (width-- > 0) {
					n = fgetc(fp);
					if (n == EOF)
						return nassigned;
					nread++;
				}
			} else {
				p = va_arg(ap, char *);
				while (width-- > 0) {
					n = fgetc(fp);
					if (n == EOF)
						return nassigned;
					*p++ = n;
					nread++;
				}
				nassigned++;
			}
			break;

		case 's':
			/* Skip leading whitespace */
			while ((n = fgetc(fp)) != EOF && isspace(n))
				nread++;
			if (n == EOF)
				return nassigned;
			
			if (flags & SUPPRESS) {
				do {
					nread++;
					if (width != 0 && --width == 0)
						break;
					n = fgetc(fp);
				} while (n != EOF && !isspace(n));
				if (n != EOF)
					ungetc(n, fp);
			} else {
				p0 = p = va_arg(ap, char *);
				do {
					*p++ = n;
					nread++;
					if (width != 0 && --width == 0)
						break;
					n = fgetc(fp);
				} while (n != EOF && !isspace(n));
				*p = '\0';
				if (n != EOF)
					ungetc(n, fp);
				nassigned++;
			}
			break;

		case '[':
			fmt = __sccl(ccltab, fmt);
			if (*fmt == 0)
				return nassigned;
			
			/* Scan character class */
			if (flags & SUPPRESS) {
				while ((n = fgetc(fp)) != EOF && ccltab[n]) {
					nread++;
					if (width != 0 && --width == 0)
						break;
				}
				if (n != EOF)
					ungetc(n, fp);
			} else {
				p0 = p = va_arg(ap, char *);
				while ((n = fgetc(fp)) != EOF && ccltab[n]) {
					*p++ = n;
					nread++;
					if (width != 0 && --width == 0)
						break;
				}
				*p = '\0';
				if (n != EOF)
					ungetc(n, fp);
				if (p == p0)
					return nassigned;
				nassigned++;
			}
			break;

		case 'n':
			if (!(flags & SUPPRESS)) {
				if (flags & LLONGINT)
					*va_arg(ap, long long *) = nread;
				else if (flags & LONGINT)
					*va_arg(ap, long *) = nread;
				else if (flags & SHORTINT)
					*va_arg(ap, short *) = nread;
				else
					*va_arg(ap, int *) = nread;
			}
			break;

		default:
			return nassigned;
		}
	}
}

int
fscanf(FILE *fp, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = vfscanf(fp, fmt, ap);
	va_end(ap);
	return ret;
}

int
scanf(const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = vfscanf(stdin, fmt, ap);
	va_end(ap);
	return ret;
}

int
sscanf(const char *str, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = vsscanf(str, fmt, ap);
	va_end(ap);
	return ret;
}

int
vsscanf(const char *str, const char *fmt, va_list ap)
{
	FILE f;
	
	/* Create fake FILE for string input */
	memset(&f, 0, sizeof(f));
	f._flags = __SRD;
	f._bf._base = f._p = (unsigned char *)str;
	f._bf._size = f._r = strlen(str);
	
	return vfscanf(&f, fmt, ap);
}

int
vscanf(const char *fmt, va_list ap)
{
	return vfscanf(stdin, fmt, ap);
}
