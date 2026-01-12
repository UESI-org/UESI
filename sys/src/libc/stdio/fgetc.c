#include <stdio.h>
#include <string.h>
#include "stdio_internal.h"

#undef fgetc
#undef getc
#undef getchar
#undef getc_unlocked
#undef getchar_unlocked

int
__srget(FILE *fp)
{
	if (__srefill(fp) == 0) {
		fp->_r--;
		return *fp->_p++;
	}
	return EOF;
}

int
fgetc(FILE *fp)
{
	return __sgetc(fp);
}

int
getc(FILE *fp)
{
	return __sgetc(fp);
}

int
getchar(void)
{
	return getc(stdin);
}

int
getc_unlocked(FILE *fp)
{
	return __sgetc(fp);
}

int
getchar_unlocked(void)
{
	return __sgetc(stdin);
}

char *
fgets(char *buf, int n, FILE *fp)
{
	size_t len;
	char *s;
	unsigned char *p, *t;

	if (n <= 0)
		return NULL;

	s = buf;
	n--;
	while (n != 0) {
		if ((len = fp->_r) <= 0) {
			if (__srefill(fp)) {
				if (s == buf)
					return NULL;
				break;
			}
			len = fp->_r;
		}
		p = fp->_p;

		if (len > (size_t)n)
			len = n;
		t = memchr((void *)p, '\n', len);
		if (t != NULL) {
			len = ++t - p;
			fp->_r -= len;
			fp->_p = t;
			(void)memcpy((void *)s, (void *)p, len);
			s[len] = '\0';
			return buf;
		}
		fp->_r -= len;
		fp->_p += len;
		(void)memcpy((void *)s, (void *)p, len);
		s += len;
		n -= len;
	}
	*s = '\0';
	return buf;
}

char *
gets(char *buf)
{
	int c;
	char *s;
	size_t count = 0;

	__attribute__((__deprecated__("gets is unsafe, use fgets instead")));

	for (s = buf; (c = getchar()) != '\n';) {
		if (c == EOF) {
			if (s == buf)
				return NULL;
			break;
		}
		*s++ = c;
		if (++count >= 4096)
			break;
	}
	*s = '\0';
	return buf;
}