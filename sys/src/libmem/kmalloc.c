#include <kmalloc.h>
#include <kfree.h>
#include <pmm.h>
#include <paging.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

extern void serial_printf(uint16_t port, const char *fmt, ...);
#define DEBUG_PORT 0x3F8

cache_t caches[CACHE_COUNT];
static bool kmalloc_initialized = false;

static const size_t cache_sizes[CACHE_COUNT] = {
    16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192
};

static inline int get_cache_index(size_t size) {
    for (int i = 0; i < CACHE_COUNT; i++) {
        if (size <= cache_sizes[i]) {
            return i;
        }
    }
    return -1; /* Size too large for caching */
}

static uint32_t calculate_objects_per_slab(size_t object_size, size_t align) {
    /* Reserve space for slab header */
    size_t header_base = sizeof(slab_t);
    
    uint32_t estimated_objects = 200; /* Start with reasonable estimate */
    size_t bitmap_size = (estimated_objects + 63) / 64 * sizeof(uint64_t);
    
    /* Calculate aligned start for objects */
    size_t header_total = header_base + bitmap_size;
    size_t aligned_start = (header_total + align - 1) & ~(align - 1);
    
    size_t usable_size = PAGE_SIZE - aligned_start;
    
    uint32_t objects = usable_size / object_size;
    
    /* Ensure we don't exceed reasonable limits */
    if (objects > 256) objects = 256;
    if (objects < 1) objects = 1;
    
    return objects;
}

static void cache_init(cache_t *cache, const char *name, size_t object_size, size_t align) {
    cache->name = name;
    cache->object_size = object_size;
    cache->align = align;
    cache->objects_per_slab = calculate_objects_per_slab(object_size, align);
    cache->partial = NULL;
    cache->full = NULL;
    cache->empty = NULL;
    cache->alloc_count = 0;
    cache->free_count = 0;
}

static slab_t *slab_create(cache_t *cache) {
    void *page = pmm_alloc();
    if (page == NULL) {
        serial_printf(DEBUG_PORT, "[kmalloc] Failed to allocate page for slab\n");
        return NULL;
    }
    
    slab_t *slab = (slab_t *)page;
    slab->next = NULL;
    slab->total_count = cache->objects_per_slab;
    slab->free_count = cache->objects_per_slab;
    
    size_t bitmap_size = (cache->objects_per_slab + 63) / 64;
    slab->free_bitmap = (uint64_t *)((uint8_t *)slab + sizeof(slab_t));
    
    /* Initialize bitmap - all objects are free (set all bits to 1) */
    for (size_t i = 0; i < bitmap_size; i++) {
        slab->free_bitmap[i] = 0xFFFFFFFFFFFFFFFFULL;
    }
    
    size_t extra_bits = (bitmap_size * 64) - cache->objects_per_slab;
    if (extra_bits > 0 && extra_bits < 64) {
        uint64_t mask = (1ULL << (64 - extra_bits)) - 1;
        slab->free_bitmap[bitmap_size - 1] = (1ULL << (64 - extra_bits)) - 1;
    }
    
    size_t header_size = sizeof(slab_t) + (bitmap_size * sizeof(uint64_t));
    size_t aligned_header = (header_size + cache->align - 1) & ~(cache->align - 1);
    slab->objects = (void *)((uint8_t *)page + aligned_header);
    
    return slab;
}

static int slab_find_free(slab_t *slab, cache_t *cache) {
    size_t bitmap_size = (cache->objects_per_slab + 63) / 64;
    
    for (size_t i = 0; i < bitmap_size; i++) {
        if (slab->free_bitmap[i] != 0) {
            /* Find first set bit */
            uint64_t bits = slab->free_bitmap[i];
            for (int bit = 0; bit < 64; bit++) {
                if (bits & (1ULL << bit)) {
                    int index = (i * 64) + bit;
                    if ((uint32_t)index < cache->objects_per_slab) {
                        return index;
                    }
                }
            }
        }
    }
    
    return -1;
}

static void slab_mark_allocated(slab_t *slab, int index) {
    size_t bitmap_index = index / 64;
    size_t bit_index = index % 64;
    slab->free_bitmap[bitmap_index] &= ~(1ULL << bit_index);
    slab->free_count--;
}

static void *cache_alloc(cache_t *cache, uint32_t flags) {
    slab_t *slab = cache->partial;
    
    /* If no partial slabs, try to get one from empty list */
    if (slab == NULL) {
        if (cache->empty != NULL) {
            slab = cache->empty;
            cache->empty = slab->next;
            slab->next = NULL;
            cache->partial = slab;
        } else {
            slab = slab_create(cache);
            if (slab == NULL) {
                return NULL;
            }
            cache->partial = slab;
        }
    }
    
    int obj_index = slab_find_free(slab, cache);
    if (obj_index < 0) {
        serial_printf(DEBUG_PORT, "[kmalloc] BUG: No free object in partial slab\n");
        return NULL;
    }
    
    size_t bitmap_index = obj_index / 64;
    size_t bit_index = obj_index % 64;
    if (!(slab->free_bitmap[bitmap_index] & (1ULL << bit_index))) {
        serial_printf(DEBUG_PORT, "[kmalloc] BUG: Attempted to allocate already-used object\n");
        return NULL;
    }
    
    void *obj = (uint8_t *)slab->objects + (obj_index * cache->object_size);
    
    slab_mark_allocated(slab, obj_index);
    
    /* If slab is now full, move it to full list */
    if (slab->free_count == 0) {
        cache->partial = slab->next;
        slab->next = cache->full;
        cache->full = slab;
    }
    
    cache->alloc_count++;
    
    if (flags & KMALLOC_ZERO) {
        memset(obj, 0, cache->object_size);
    }
    
    return obj;
}

void kmalloc_init(void) {
    if (kmalloc_initialized) {
        return;
    }
    
    serial_printf(DEBUG_PORT, "[kmalloc] Initializing kernel memory allocator\n");
    
    static const char *cache_names[CACHE_COUNT] = {
        "kmalloc-16", "kmalloc-32", "kmalloc-64", "kmalloc-128",
        "kmalloc-256", "kmalloc-512", "kmalloc-1024", "kmalloc-2048",
        "kmalloc-4096", "kmalloc-8192"
    };
    
    for (int i = 0; i < CACHE_COUNT; i++) {
        cache_init(&caches[i], cache_names[i], cache_sizes[i], cache_sizes[i]);
        serial_printf(DEBUG_PORT, "[kmalloc] Created cache: %s (%u bytes, %u objs/slab)\n",
                     cache_names[i], (unsigned int)cache_sizes[i], 
                     caches[i].objects_per_slab);
    }
    
    kmalloc_initialized = true;
    serial_printf(DEBUG_PORT, "[kmalloc] Initialization complete\n");
}

void *kmalloc_flags(size_t size, uint32_t flags) {
    if (!kmalloc_initialized) {
        kmalloc_init();
    }
    
    if (size == 0) {
        return NULL;
    }
    
    int cache_idx = get_cache_index(size);
    
    if (cache_idx < 0) {
        /* Size too large for slab allocator, use direct page allocation */
        size_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
        void *ptr = pmm_alloc_contiguous(num_pages);
        
        if (ptr != NULL && (flags & KMALLOC_ZERO)) {
            memset(ptr, 0, num_pages * PAGE_SIZE);
        }
        
        return ptr;
    }
    
    return cache_alloc(&caches[cache_idx], flags);
}

void *kmalloc(size_t size) {
    return kmalloc_flags(size, 0);
}

void *kmalloc_aligned(size_t size, size_t align) {
    if (align <= KMALLOC_MIN_SIZE) {
        return kmalloc(size);
    }
    
    int cache_idx = get_cache_index(size > align ? size : align);
    if (cache_idx < 0) {

        size_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
        return pmm_alloc_contiguous(num_pages);
    }
    
    return kmalloc(cache_sizes[cache_idx]);
}

size_t kmalloc_size(void *ptr) {
    if (ptr == NULL) {
        return 0;
    }

    for (int i = 0; i < CACHE_COUNT; i++) {
        cache_t *cache = &caches[i];
        
        /* Check all slabs in this cache */
        slab_t *slab = cache->partial;
        while (slab != NULL) {
            if ((uintptr_t)ptr >= (uintptr_t)slab &&
                (uintptr_t)ptr < (uintptr_t)slab + PAGE_SIZE) {
                return cache->object_size;
            }
            slab = slab->next;
        }
        
        slab = cache->full;
        while (slab != NULL) {
            if ((uintptr_t)ptr >= (uintptr_t)slab &&
                (uintptr_t)ptr < (uintptr_t)slab + PAGE_SIZE) {
                return cache->object_size;
            }
            slab = slab->next;
        }
    }
    
    return 0; /* Unknown size */
}

void kmalloc_stats(void) {
    serial_printf(DEBUG_PORT, "\n=== Kernel Memory Allocator Statistics ===\n");
    
    uint64_t total_allocs = 0;
    uint64_t total_frees = 0;
    
    for (int i = 0; i < CACHE_COUNT; i++) {
        cache_t *cache = &caches[i];
        
        int partial_count = 0, full_count = 0, empty_count = 0;
        slab_t *slab;
        
        for (slab = cache->partial; slab != NULL; slab = slab->next) partial_count++;
        for (slab = cache->full; slab != NULL; slab = slab->next) full_count++;
        for (slab = cache->empty; slab != NULL; slab = slab->next) empty_count++;
        
        serial_printf(DEBUG_PORT, "%s:\n", cache->name);
        serial_printf(DEBUG_PORT, "  Object size: %u bytes\n", (unsigned int)cache->object_size);
        serial_printf(DEBUG_PORT, "  Objects per slab: %u\n", cache->objects_per_slab);
        serial_printf(DEBUG_PORT, "  Slabs: %d partial, %d full, %d empty\n",
                     partial_count, full_count, empty_count);
        serial_printf(DEBUG_PORT, "  Allocations: %llu, Frees: %llu\n",
                     cache->alloc_count, cache->free_count);
        
        total_allocs += cache->alloc_count;
        total_frees += cache->free_count;
    }
    
    serial_printf(DEBUG_PORT, "\nTotal allocations: %llu\n", total_allocs);
    serial_printf(DEBUG_PORT, "Total frees: %llu\n", total_frees);
    serial_printf(DEBUG_PORT, "Outstanding: %llu\n", total_allocs - total_frees);
    serial_printf(DEBUG_PORT, "==========================================\n\n");
}