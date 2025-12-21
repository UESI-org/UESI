#include <stdlib.h>

div_t
div(int numer, int denom)
{
	div_t result;
	result.quot = numer / denom;
	result.rem = numer % denom;
	return result;
}

ldiv_t
ldiv(long numer, long denom)
{
	ldiv_t result;
	result.quot = numer / denom;
	result.rem = numer % denom;
	return result;
}

lldiv_t
lldiv(long long numer, long long denom)
{
	lldiv_t result;
	result.quot = numer / denom;
	result.rem = numer % denom;
	return result;
}
