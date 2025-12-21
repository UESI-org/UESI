#include <stdlib.h>

int
abs(int j)
{
	return (j < 0) ? -j : j;
}

long
labs(long j)
{
	return (j < 0) ? -j : j;
}

long long
llabs(long long j)
{
	return (j < 0) ? -j : j;
}
