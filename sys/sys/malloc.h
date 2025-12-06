#ifndef _MALLOC_H_
#define _MALLOC_H_

#include <sys/cdefs.h>
#include <sys/types.h>

__BEGIN_DECLS

void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void *reallocarray(void *ptr, size_t nmemb, size_t size);
void free(void *ptr);
void *aligned_alloc(size_t alignment, size_t size);
int posix_memalign(void **memptr, size_t alignment, size_t size);

__END_DECLS

#endif