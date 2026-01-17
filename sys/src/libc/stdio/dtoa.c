#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

static inline int
is_nan(double x)
{
	return x != x;
}

static inline int
is_inf(double x)
{
	return x == HUGE_VAL || x == -HUGE_VAL;
}

int
dtoa_f(char *buf, size_t bufsize, double value, int prec, int flags, char *sign)
{
	uint64_t intpart, fracpart;
	double temp;
	int i, len = 0;
	char *p = buf;
	
	/* Handle special values */
	if (is_nan(value)) {
		return snprintf(buf, bufsize, "nan");
	}
	if (is_inf(value)) {
		if (value < 0) *sign = '-';
		return snprintf(buf, bufsize, "inf");
	}
	
	/* Handle sign */
	if (value < 0) {
		*sign = '-';
		value = -value;
	} else if (value == 0.0 && 1.0/value < 0) {
		/* Negative zero */
		*sign = '-';
	}
	
	/* Default precision */
	if (prec < 0) prec = 6;
	if (prec > 17) prec = 17; /* Limit for double precision */
	
	/* Round to the desired precision */
	temp = value;
	for (i = 0; i < prec; i++)
		temp *= 10.0;
	temp += 0.5;
	
	/* Get integer and fractional parts */
	intpart = (uint64_t)(temp / pow(10.0, prec));
	fracpart = (uint64_t)temp % (uint64_t)pow(10.0, prec);
	
	/* Format integer part */
	if (intpart == 0) {
		*p++ = '0';
		len++;
	} else {
		char tmp[32];
		int j = 0;
		uint64_t n = intpart;
		
		while (n > 0) {
			tmp[j++] = '0' + (n % 10);
			n /= 10;
		}
		
		/* Reverse */
		while (j > 0) {
			*p++ = tmp[--j];
			len++;
		}
	}
	
	/* Decimal point */
	if (prec > 0 || (flags & 0x001 /* ALT */)) {
		*p++ = '.';
		len++;
		
		/* Fractional part */
		if (prec > 0) {
			for (i = prec - 1; i >= 0; i--) {
				int digit = (fracpart / (uint64_t)pow(10.0, i)) % 10;
				*p++ = '0' + digit;
				len++;
			}
			
			/* Remove trailing zeros unless ALT flag set */
			if (!(flags & 0x001)) {
				while (p[-1] == '0' && p[-2] != '.') {
					p--;
					len--;
				}
			}
		}
	}
	
	*p = '\0';
	return len;
}

int
dtoa_e(char *buf, size_t bufsize, double value, int prec, int flags, char *sign, int uppercase)
{
	int exp = 0;
	double mant;
	int len = 0;
	char *p = buf;
	
	/* Handle special values */
	if (is_nan(value)) {
		return snprintf(buf, bufsize, uppercase ? "NAN" : "nan");
	}
	if (is_inf(value)) {
		if (value < 0) *sign = '-';
		return snprintf(buf, bufsize, uppercase ? "INF" : "inf");
	}
	
	/* Handle sign */
	if (value < 0) {
		*sign = '-';
		value = -value;
	} else if (value == 0.0 && 1.0/value < 0) {
		*sign = '-';
	}
	
	/* Default precision */
	if (prec < 0) prec = 6;
	
	/* Normalize to 1.xxx * 10^exp */
	if (value != 0.0) {
		exp = (int)floor(log10(value));
		mant = value / pow(10.0, exp);
	} else {
		exp = 0;
		mant = 0.0;
	}
	
	/* Format mantissa using dtoa_f */
	len = dtoa_f(buf, bufsize, mant, prec, flags, sign);
	p = buf + len;
	
	/* Add exponent */
	*p++ = uppercase ? 'E' : 'e';
	*p++ = (exp < 0) ? '-' : '+';
	if (exp < 0) exp = -exp;
	
	/* Always use at least 2 digits for exponent */
	if (exp < 10) {
		*p++ = '0';
		*p++ = '0' + exp;
	} else if (exp < 100) {
		*p++ = '0' + (exp / 10);
		*p++ = '0' + (exp % 10);
	} else {
		char tmp[8];
		int i = 0;
		while (exp > 0) {
			tmp[i++] = '0' + (exp % 10);
			exp /= 10;
		}
		while (i > 0) {
			*p++ = tmp[--i];
		}
	}
	
	*p = '\0';
	return p - buf;
}

int
dtoa_g(char *buf, size_t bufsize, double value, int prec, int flags, char *sign, int uppercase)
{
	int exp;
	
	/* Handle special values */
	if (is_nan(value)) {
		return snprintf(buf, bufsize, uppercase ? "NAN" : "nan");
	}
	if (is_inf(value)) {
		if (value < 0) *sign = '-';
		return snprintf(buf, bufsize, uppercase ? "INF" : "inf");
	}
	
	/* Default precision */
	if (prec == 0) prec = 1;
	if (prec < 0) prec = 6;
	
	/* Get exponent */
	if (value == 0.0) {
		exp = 0;
	} else {
		double absval = (value < 0) ? -value : value;
		exp = (int)floor(log10(absval));
	}
	
	/* Use %e if exponent < -4 or >= precision */
	if (exp < -4 || exp >= prec) {
		/* Use %e style, precision is number of significant digits - 1 */
		return dtoa_e(buf, bufsize, value, prec - 1, flags & ~0x001, sign, uppercase);
	} else {
		/* Use %f style, precision is number of digits after decimal point */
		int fprec = prec - 1 - exp;
		if (fprec < 0) fprec = 0;
		return dtoa_f(buf, bufsize, value, fprec, flags & ~0x001, sign);
	}
}