#include <stdlib.h>

void *
bsearch(const void *key,
        const void *base,
        size_t nmemb,
        size_t size,
        int (*compar)(const void *, const void *))
{
	const char *base_ptr = (const char *)base;
	size_t low = 0, high = nmemb;

	while (low < high) {
		size_t mid = low + (high - low) / 2;
		const void *mid_ptr = base_ptr + mid * size;
		int cmp = compar(key, mid_ptr);

		if (cmp < 0) {
			high = mid;
		} else if (cmp > 0) {
			low = mid + 1;
		} else {
			return (void *)mid_ptr;
		}
	}

	return NULL;
}
