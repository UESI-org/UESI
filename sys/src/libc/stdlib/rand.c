#include <stdlib.h>

static unsigned int rand_seed = 1;

int
rand(void)
{
	rand_seed = rand_seed * 1103515245 + 12345;
	return (int)((rand_seed / 65536) % (RAND_MAX + 1));
}

void
srand(unsigned int seed)
{
	rand_seed = seed;
}

#if __POSIX_VISIBLE >= 200112
int
rand_r(unsigned int *seedp)
{
	unsigned int next = *seedp;
	next = next * 1103515245 + 12345;
	*seedp = next;
	return (int)((next / 65536) % (RAND_MAX + 1));
}
#endif
