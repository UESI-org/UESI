#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <string.h>

static double
parse_float(const char *str, char **endptr)
{
	const char *s = str;
	double result = 0.0;
	double frac = 0.0;
	double div = 1.0;
	int exp = 0;
	int exp_sign = 1;
	int sign = 1;
	int has_digits = 0;
	
	/* Skip whitespace */
	while (isspace((unsigned char)*s))
		s++;
	
	/* Handle sign */
	if (*s == '-') {
		sign = -1;
		s++;
	} else if (*s == '+') {
		s++;
	}
	
	/* Check for inf/nan */
	if (strncasecmp(s, "inf", 3) == 0) {
		s += 3;
		if (strncasecmp(s, "inity", 5) == 0)
			s += 5;
		if (endptr)
			*endptr = (char *)s;
		return sign * HUGE_VAL;
	}
	
	if (strncasecmp(s, "nan", 3) == 0) {
		s += 3;
		/* Skip optional (n-char-sequence) */
		if (*s == '(') {
			s++;
			while (*s && *s != ')')
				s++;
			if (*s == ')')
				s++;
		}
		if (endptr)
			*endptr = (char *)s;
		return NAN;
	}
	
	/* Integer part */
	while (isdigit((unsigned char)*s)) {
		result = result * 10.0 + (*s - '0');
		s++;
		has_digits = 1;
	}
	
	/* Fractional part */
	if (*s == '.') {
		s++;
		while (isdigit((unsigned char)*s)) {
			frac = frac * 10.0 + (*s - '0');
			div *= 10.0;
			s++;
			has_digits = 1;
		}
	}
	
	if (!has_digits) {
		if (endptr)
			*endptr = (char *)str;
		return 0.0;
	}
	
	result += frac / div;
	
	/* Exponent */
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
			if (exp_sign < 0)
				s--;
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
		result *= pow(10.0, exp);
	}
	
	if (endptr)
		*endptr = (char *)s;
	
	return sign * result;
}