#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <string.h>

#ifndef HUGE_VAL
#define HUGE_VAL (__builtin_huge_val())
#endif

#ifndef NAN
#define NAN (__builtin_nan(""))
#endif

static double
parse_hexfloat(const char **strp, char **endptr)
{
	/* TODO: Implement hexadecimal float parsing (0x1.fp+3) */
	if (endptr) {
		*endptr = (char *)*strp;
	}
	errno = EINVAL;
	return 0.0;
}

double
strtod(const char *__restrict nptr, char **__restrict endptr)
{
	const char *s = nptr;
	int sign = 1;
	double result = 0.0;
	int exp = 0;
	int exp_sign = 1;
	int digits = 0;

	while (isspace((unsigned char)*s)) {
		s++;
	}

	if (*s == '-') {
		sign = -1;
		s++;
	} else if (*s == '+') {
		s++;
	}

	/* Check for special values */
	if (strncasecmp(s, "inf", 3) == 0) {
		s += 3;
		if (strncasecmp(s, "inity", 5) == 0) {
			s += 5;
		}
		if (endptr) {
			*endptr = (char *)s;
		}
		return sign * HUGE_VAL;
	}

	if (strncasecmp(s, "nan", 3) == 0) {
		s += 3;
		/* Skip optional (n-char-sequence) */
		if (*s == '(') {
			s++;
			while (*s && *s != ')') {
				s++;
			}
			if (*s == ')') {
				s++;
			}
		}
		if (endptr) {
			*endptr = (char *)s;
		}
		return NAN;
	}

	if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
		return parse_hexfloat(&s, endptr);
	}

	while (isdigit((unsigned char)*s)) {
		result = result * 10.0 + (*s - '0');
		s++;
		digits++;
	}

	if (*s == '.') {
		s++;
		double frac = 0.1;
		while (isdigit((unsigned char)*s)) {
			result += (*s - '0') * frac;
			frac *= 0.1;
			s++;
			digits++;
		}
	}

	if (digits == 0) {
		if (endptr) {
			*endptr = (char *)nptr;
		}
		return 0.0;
	}

	if (*s == 'e' || *s == 'E') {
		s++;

		if (*s == '-') {
			exp_sign = -1;
			s++;
		} else if (*s == '+') {
			s++;
		}

		if (!isdigit((unsigned char)*s)) {
			/* Invalid exponent */
			s--;
			if (exp_sign < 0) s--;
			goto done;
		}

		while (isdigit((unsigned char)*s)) {
			exp = exp * 10 + (*s - '0');
			s++;
		}

		exp *= exp_sign;
	}

done:
	/* Apply exponent */
	if (exp != 0) {
		/* Use pow or manual calculation */
		double exp_mult = 1.0;
		int abs_exp = (exp < 0) ? -exp : exp;
		
		for (int i = 0; i < abs_exp; i++) {
			exp_mult *= 10.0;
		}

		if (exp < 0) {
			result /= exp_mult;
		} else {
			result *= exp_mult;
		}

		/* Check for overflow/underflow */
		if (result == HUGE_VAL || result == -HUGE_VAL) {
			errno = ERANGE;
		}
	}

	if (endptr) {
		*endptr = (char *)s;
	}

	return sign * result;
}

float
strtof(const char *__restrict nptr, char **__restrict endptr)
{
	return (float)strtod(nptr, endptr);
}

long double
strtold(const char *__restrict nptr, char **__restrict endptr)
{
	/* For now, same as double */
	return (long double)strtod(nptr, endptr);
}

double
atof(const char *nptr)
{
	return strtod(nptr, NULL);
}