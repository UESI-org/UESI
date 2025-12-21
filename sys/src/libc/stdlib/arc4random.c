#include <stdlib.h>
#include <stdint.h>

#if __BSD_VISIBLE

/* Cryptographically secure random - stub for now */
/* TODO: Implement proper CSPRNG */

uint32_t
arc4random(void)
{
	return (uint32_t)rand() ^ ((uint32_t)rand() << 16);
}

void
arc4random_buf(void *buf, size_t nbytes)
{
	uint8_t *p = (uint8_t *)buf;
	while (nbytes--) {
		*p++ = (uint8_t)arc4random();
	}
}

uint32_t
arc4random_uniform(uint32_t upper_bound)
{
	if (upper_bound < 2) {
		return 0;
	}
	
	uint32_t min = (0xFFFFFFFF - upper_bound + 1) % upper_bound;
	uint32_t r;
	
	do {
		r = arc4random();
	} while (r < min);
	
	return r % upper_bound;
}

#endif /* __BSD_VISIBLE */
