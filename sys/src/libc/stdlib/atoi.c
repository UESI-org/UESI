#include <stdlib.h>

int
atoi(const char *nptr)
{
	return (int)strtol(nptr, NULL, 10);
}

long
atol(const char *nptr)
{
	return strtol(nptr, NULL, 10);
}

long long
atoll(const char *nptr)
{
	return strtoll(nptr, NULL, 10);
}
