#include <stdlib.h>
#include <limits.h>
#include <errno.h>

extern int char_to_digit(char c, int base);

#ifndef _KERNEL
#endif

static inline int
is_space(int c)
{
	return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
	        c == '\v');
}

unsigned long long
strtoull(const char *__restrict nptr, char **__restrict endptr, int base)
{
	const char *s = nptr;
	unsigned long long acc = 0;
	int c, neg = 0, any;
	unsigned long long cutoff, cutlim;

	while (is_space((unsigned char)*s)) {
		s++;
	}

	if (*s == '-') {
		neg = 1;
		s++;
	} else if (*s == '+') {
		s++;
	}

	if ((base == 0 || base == 16) && *s == '0' &&
	    (s[1] == 'x' || s[1] == 'X')) {
		s += 2;
		base = 16;
	} else if (base == 0) {
		base = (*s == '0') ? 8 : 10;
	}

	if (base < 2 || base > 36) {
		if (endptr != NULL) {
			*endptr = (char *)nptr;
		}
		errno = EINVAL;
		return 0;
	}

	cutoff = ULLONG_MAX / base;
	cutlim = ULLONG_MAX % base;

	for (acc = 0, any = 0;; s++) {
		c = char_to_digit(*s, base);
		if (c < 0) {
			break;
		}

		if (any < 0 || acc > cutoff ||
		    (acc == cutoff && (unsigned long long)c > cutlim)) {
			any = -1;
			errno = ERANGE;
			acc = ULLONG_MAX;
		} else {
			any = 1;
			acc = acc * base + c;
		}
	}

	if (endptr != NULL) {
		*endptr = (char *)(any ? s : nptr);
	}

	return neg ? -acc : acc;
}