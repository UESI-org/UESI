#include <stdlib.h>

#ifndef _KERNEL

/* TODO: Implement proper floating point conversion */
/* For now, basic integer conversion */

double
atof(const char *nptr)
{
	return strtod(nptr, NULL);
}

double
strtod(const char *__restrict nptr, char **__restrict endptr)
{
	long val = strtol(nptr, endptr, 10);
	return (double)val;
}

float
strtof(const char *__restrict nptr, char **__restrict endptr)
{
	return (float)strtod(nptr, endptr);
}

long double
strtold(const char *__restrict nptr, char **__restrict endptr)
{
	return (long double)strtod(nptr, endptr);
}

#endif