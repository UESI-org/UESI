#include <stdlib.h>
#include <stddef.h>

size_t
mbstowcs(wchar_t *__restrict pwcs, const char *__restrict s, size_t n)
{
	size_t count = 0;
	while (count < n && *s != '\0') {
		*pwcs++ = (wchar_t)(unsigned char)*s++;
		count++;
	}
	if (count < n) {
		*pwcs = L'\0';
	}
	return count;
}

size_t
wcstombs(char *__restrict s, const wchar_t *__restrict pwcs, size_t n)
{
	size_t count = 0;
	while (count < n && *pwcs != L'\0') {
		*s++ = (char)*pwcs++;
		count++;
	}
	if (count < n) {
		*s = '\0';
	}
	return count;
}
