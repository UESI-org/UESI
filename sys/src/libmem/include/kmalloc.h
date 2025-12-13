#ifndef _KMALLOC_H_
#define _KMALLOC_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define KMALLOC_ZERO (1 << 0)   /* Zero the allocated memory */
#define KMALLOC_ATOMIC (1 << 1) /* Cannot sleep/block */

#define KMALLOC_MIN_SIZE 16
#define KMALLOC_MAX_SIZE 8192

enum {
	CACHE_16 = 0,
	CACHE_32,
	CACHE_64,
	CACHE_128,
	CACHE_256,
	CACHE_512,
	CACHE_1024,
	CACHE_2048,
	CACHE_4096,
	CACHE_8192,
	CACHE_COUNT
};

typedef struct slab {
	struct slab *next;     /* Next slab in list */
	void *objects;         /* Pointer to first object */
	uint32_t free_count;   /* Number of free objects */
	uint32_t total_count;  /* Total objects in slab */
	uint64_t *free_bitmap; /* Bitmap of free objects */
} slab_t;

typedef struct cache {
	const char *name;          /* Cache name for debugging */
	size_t object_size;        /* Size of each object */
	size_t align;              /* Alignment requirement */
	uint32_t objects_per_slab; /* Objects per slab */
	slab_t *partial;           /* Slabs with free objects */
	slab_t *full;              /* Completely full slabs */
	slab_t *empty;             /* Completely empty slabs */
	uint64_t alloc_count;      /* Total allocations */
	uint64_t free_count;       /* Total frees */
} cache_t;

void kmalloc_init(void);

void *kmalloc(size_t size);
void *kmalloc_flags(size_t size, uint32_t flags);

void *kmalloc_aligned(size_t size, size_t align);

size_t kmalloc_size(void *ptr);

void kmalloc_stats(void);

extern cache_t caches[CACHE_COUNT];

#endif