#include <stdlib.h>
#include <stddef.h>

int
mblen(const char *s, size_t n)
{
	if (s == NULL) {
		return 0; /* No state dependency */
	}
	if (n == 0 || *s == '\0') {
		return 0;
	}
	return 1; /* ASCII only */
}

int
mbtowc(wchar_t *__restrict pwc, const char *__restrict s, size_t n)
{
	if (s == NULL) {
		return 0; /* No state dependency */
	}
	if (n == 0) {
		return -1;
	}
	if (pwc != NULL) {
		*pwc = (wchar_t)(unsigned char)*s;
	}
	return (*s != '\0') ? 1 : 0;
}

int
wctomb(char *s, wchar_t wchar)
{
	if (s == NULL) {
		return 0; /* No state dependency */
	}
	*s = (char)wchar;
	return 1;
}
