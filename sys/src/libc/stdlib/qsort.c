#include <stdlib.h>
#include <stdbool.h>

static void
swap_bytes(void *a, void *b, size_t size)
{
	unsigned char *p1 = (unsigned char *)a;
	unsigned char *p2 = (unsigned char *)b;
	
	while (size--) {
		unsigned char tmp = *p1;
		*p1++ = *p2;
		*p2++ = tmp;
	}
}

void
qsort(void *base, size_t nmemb, size_t size,
      int (*compar)(const void *, const void *))
{
	char *base_ptr = (char *)base;
	
	if (nmemb < 2) {
		return;
	}
	
	/* Simple quicksort implementation */
	size_t pivot = nmemb / 2;
	size_t i = 0, j = nmemb - 1;
	
	while (true) {
		while (i < pivot && compar(base_ptr + i * size, base_ptr + pivot * size) < 0) {
			i++;
		}
		while (j > pivot && compar(base_ptr + j * size, base_ptr + pivot * size) > 0) {
			j--;
		}
		
		if (i >= j) {
			break;
		}
		
		swap_bytes(base_ptr + i * size, base_ptr + j * size, size);
		
		if (i == pivot) {
			pivot = j;
		} else if (j == pivot) {
			pivot = i;
		}
	}
	
	/* Recursively sort partitions */
	if (pivot > 0) {
		qsort(base_ptr, pivot, size, compar);
	}
	if (pivot + 1 < nmemb) {
		qsort(base_ptr + (pivot + 1) * size, nmemb - pivot - 1, size, compar);
	}
}
