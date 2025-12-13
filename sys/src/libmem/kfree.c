#include <kfree.h>
#include <kmalloc.h>
#include <pmm.h>
#include <stdint.h>
#include <stdbool.h>

extern void serial_printf(uint16_t port, const char *fmt, ...);
#define DEBUG_PORT 0x3F8

static void
slab_mark_free(slab_t *slab, int index)
{
	size_t bitmap_index = index / 64;
	size_t bit_index = index % 64;
	slab->free_bitmap[bitmap_index] |= (1ULL << bit_index);
	slab->free_count++;
}

static bool
find_allocation(void *ptr,
                cache_t **out_cache,
                slab_t **out_slab,
                int *out_index)
{
	if (ptr == NULL) {
		return false;
	}

	for (int i = 0; i < CACHE_COUNT; i++) {
		cache_t *cache = &caches[i];

		slab_t *slab = cache->partial;
		while (slab != NULL) {
			if ((uintptr_t)ptr >= (uintptr_t)slab &&
			    (uintptr_t)ptr < (uintptr_t)slab + PAGE_SIZE) {
				if ((uintptr_t)ptr < (uintptr_t)slab->objects) {
					serial_printf(
					    DEBUG_PORT,
					    "[kfree] Invalid pointer in slab "
					    "header region\n");
					return false;
				}

				/* Calculate object index */
				uintptr_t offset =
				    (uintptr_t)ptr - (uintptr_t)slab->objects;

				if (offset % cache->object_size != 0) {
					serial_printf(
					    DEBUG_PORT,
					    "[kfree] Pointer not aligned to "
					    "object boundary\n");
					return false;
				}

				int index = offset / cache->object_size;

				if (index >= 0 &&
				    (uint32_t)index < cache->objects_per_slab) {
					*out_cache = cache;
					*out_slab = slab;
					*out_index = index;
					return true;
				}
			}
			slab = slab->next;
		}

		/* Check full slabs */
		slab = cache->full;
		while (slab != NULL) {
			if ((uintptr_t)ptr >= (uintptr_t)slab &&
			    (uintptr_t)ptr < (uintptr_t)slab + PAGE_SIZE) {
				if ((uintptr_t)ptr < (uintptr_t)slab->objects) {
					serial_printf(
					    DEBUG_PORT,
					    "[kfree] Invalid pointer in slab "
					    "header region\n");
					return false;
				}

				uintptr_t offset =
				    (uintptr_t)ptr - (uintptr_t)slab->objects;

				if (offset % cache->object_size != 0) {
					serial_printf(
					    DEBUG_PORT,
					    "[kfree] Pointer not aligned to "
					    "object boundary\n");
					return false;
				}

				int index = offset / cache->object_size;

				if (index >= 0 &&
				    (uint32_t)index < cache->objects_per_slab) {
					*out_cache = cache;
					*out_slab = slab;
					*out_index = index;
					return true;
				}
			}
			slab = slab->next;
		}
	}

	return false;
}

static void
slab_remove_from_list(slab_t **list, slab_t *slab)
{
	if (*list == slab) {
		*list = slab->next;
		return;
	}

	slab_t *current = *list;
	while (current != NULL && current->next != slab) {
		current = current->next;
	}

	if (current != NULL) {
		current->next = slab->next;
	}
}

static bool
is_object_free(slab_t *slab, int index)
{
	size_t bitmap_index = index / 64;
	size_t bit_index = index % 64;
	return (slab->free_bitmap[bitmap_index] & (1ULL << bit_index)) != 0;
}

static void
cache_free(cache_t *cache, slab_t *slab, int obj_index)
{
	if (is_object_free(slab, obj_index)) {
		serial_printf(
		    DEBUG_PORT,
		    "[kfree] WARNING: Double-free detected in cache %s\n",
		    cache->name);
		return;
	}

	slab_mark_free(slab, obj_index);
	cache->free_count++;

	bool was_full = (slab->free_count == 1); /* Was full before this free */
	bool is_empty = (slab->free_count == slab->total_count);

	if (was_full) {
		slab_remove_from_list(&cache->full, slab);
		slab->next = cache->partial;
		cache->partial = slab;
	} else if (is_empty) {
		slab_remove_from_list(&cache->partial, slab);

		int empty_count = 0;
		slab_t *temp = cache->empty;
		while (temp != NULL && empty_count < 2) {
			empty_count++;
			temp = temp->next;
		}

		if (empty_count < 2) {
			slab->next = cache->empty;
			cache->empty = slab;
		} else {
			pmm_free(slab);
		}
	}
	/* If it was partial and is still partial, no list movement needed */
}

bool
kfree_validate(void *ptr)
{
	if (ptr == NULL) {
		return true; /* NULL is valid to free */
	}

	cache_t *cache;
	slab_t *slab;
	int index;

	return find_allocation(ptr, &cache, &slab, &index);
}

void
kfree(void *ptr)
{
	if (ptr == NULL) {
		return;
	}

	cache_t *cache;
	slab_t *slab;
	int obj_index;

	if (find_allocation(ptr, &cache, &slab, &obj_index)) {
		cache_free(cache, slab, obj_index);
		return;
	}

	pmm_free(ptr);
}