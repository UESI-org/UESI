#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#ifndef _KERNEL
#endif

#if __BSD_VISIBLE

void *
recallocarray(void *ptr, size_t oldnmemb, size_t nmemb, size_t size)
{
	if (nmemb != 0 && size > SIZE_MAX / nmemb) {
		errno = ENOMEM;
		return NULL;
	}
	
	size_t newsize = nmemb * size;
	size_t oldsize = oldnmemb * size;
	
	void *newptr = realloc(ptr, newsize);
	if (newptr != NULL && newsize > oldsize) {
		memset((char *)newptr + oldsize, 0, newsize - oldsize);
	}
	
	return newptr;
}

void
freezero(void *ptr, size_t size)
{
	if (ptr != NULL) {
		extern void explicit_bzero(void *buf, size_t len);
		explicit_bzero(ptr, size);
		free(ptr);
	}
}

#endif /* __BSD_VISIBLE */
