#include <sys/mman.h>
#include <sys/malloc.h>
#include <sys/types.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#ifndef _KERNEL
#endif

#ifdef _KERNEL
extern int64_t sys_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
extern int64_t sys_munmap(void *addr, size_t length);

static inline void *
kernel_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
	int64_t ret = sys_mmap(addr, len, prot, flags, fd, off);
	return (ret < 0) ? MAP_FAILED : (void *)ret;
}

static inline int
kernel_munmap(void *addr, size_t len)
{
	return (int)sys_munmap(addr, len);
}

#define USER_MMAP kernel_mmap
#define USER_MUNMAP kernel_munmap
#else
extern void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
extern int munmap(void *addr, size_t length);

#define USER_MMAP mmap
#define USER_MUNMAP munmap
#endif

#define PAGE_SIZE 4096
#define ALIGNMENT 16
#define MIN_BLOCK_SIZE (sizeof(block_t))
#define LARGE_THRESHOLD (PAGE_SIZE / 2)
#define ARENA_SIZE (PAGE_SIZE * 64)  /* 256KB arenas */

#define NUM_SIZE_CLASSES 8
static const size_t size_classes[NUM_SIZE_CLASSES] = {
	16, 32, 64, 128, 256, 512, 1024, 2048
};

typedef struct block {
	size_t size;           /* Size including header (low bit = allocated) */
	struct block *next;    /* Next in free list */
	struct block *prev;    /* Previous in free list */
	uint32_t magic;        /* Corruption detection */
	uint32_t pad;          /* Alignment padding */
} block_t;

typedef struct {
	size_t size;           /* Same as header size */
} footer_t;

#define BLOCK_MAGIC 0xDEADC0DE
#define SIZE_MASK (~(size_t)0x1)
#define ALLOCATED_BIT 0x1

#define BLOCK_SIZE(b) ((b)->size & SIZE_MASK)
#define IS_ALLOCATED(b) ((b)->size & ALLOCATED_BIT)
#define SET_ALLOCATED(b, sz) ((b)->size = ((sz) | ALLOCATED_BIT))
#define SET_FREE(b, sz) ((b)->size = ((sz) & SIZE_MASK))

#define GET_FOOTER(b) ((footer_t *)((char *)(b) + BLOCK_SIZE(b) - sizeof(footer_t)))

#define NEXT_BLOCK(b) ((block_t *)((char *)(b) + BLOCK_SIZE(b)))
#define PREV_BLOCK(b) ((block_t *)((char *)(b) - ((footer_t *)((char *)(b) - sizeof(footer_t)))->size))

typedef struct arena {
	void *base;
	size_t size;
	size_t used;
	size_t free_bytes;
	struct arena *next;
	struct arena *prev;
} arena_t;

static block_t *size_class_bins[NUM_SIZE_CLASSES] = {NULL};
static block_t *general_free_list = NULL;
static arena_t *arena_list = NULL;
static volatile int malloc_lock = 0;

static inline void
lock_malloc(void)
{
	while (__sync_lock_test_and_set(&malloc_lock, 1)) {
		/* Pause for hyper-threading efficiency */
		__asm__ volatile("pause" ::: "memory");
	}
}

static inline void
unlock_malloc(void)
{
	__sync_lock_release(&malloc_lock);
}

static inline size_t
align_up(size_t size, size_t align)
{
	return (size + align - 1) & ~(align - 1);
}

static int
get_size_class(size_t size)
{
	for (int i = 0; i < NUM_SIZE_CLASSES; i++) {
		if (size <= size_classes[i]) {
			return i;
		}
	}
	return -1;
}

static void
remove_from_free_list(block_t *block)
{
	if (block->prev) {
		block->prev->next = block->next;
	}
	if (block->next) {
		block->next->prev = block->prev;
	}

	/* Update list head if this was the first block */
	int sc = get_size_class(BLOCK_SIZE(block));
	if (sc >= 0 && size_class_bins[sc] == block) {
		size_class_bins[sc] = block->next;
	} else if (general_free_list == block) {
		general_free_list = block->next;
	}

	block->next = block->prev = NULL;
}

static void
add_to_free_list(block_t *block)
{
	size_t size = BLOCK_SIZE(block);
	int sc = get_size_class(size);

	if (sc >= 0) {
		/* Add to size class bin */
		block->next = size_class_bins[sc];
		block->prev = NULL;
		if (block->next) {
			block->next->prev = block;
		}
		size_class_bins[sc] = block;
	} else {
		/* Add to general free list (sorted by size) */
		block_t **head = &general_free_list;
		block_t *curr = *head;
		block_t *prev = NULL;

		/* Insert in size order */
		while (curr && BLOCK_SIZE(curr) < size) {
			prev = curr;
			curr = curr->next;
		}

		block->next = curr;
		block->prev = prev;

		if (prev) {
			prev->next = block;
		} else {
			*head = block;
		}

		if (curr) {
			curr->prev = block;
		}
	}
}

static block_t *
coalesce(block_t *block)
{
	size_t size = BLOCK_SIZE(block);
	block_t *next = NEXT_BLOCK(block);

	/* Check if we can coalesce with next block */
	if ((char *)next < (char *)block + ARENA_SIZE && !IS_ALLOCATED(next)) {
		remove_from_free_list(next);
		size += BLOCK_SIZE(next);
		SET_FREE(block, size);
		GET_FOOTER(block)->size = size;
	}

	/* Check if we can coalesce with previous block */
	if ((char *)block > (char *)block - sizeof(footer_t)) {
		footer_t *prev_footer = (footer_t *)((char *)block - sizeof(footer_t));
		if (prev_footer->size && !(prev_footer->size & ALLOCATED_BIT)) {
			block_t *prev = PREV_BLOCK(block);
			remove_from_free_list(prev);
			size = BLOCK_SIZE(prev) + size;
			SET_FREE(prev, size);
			GET_FOOTER(prev)->size = size;
			block = prev;
		}
	}

	return block;
}

static void
split_block(block_t *block, size_t needed_size)
{
	size_t total_size = BLOCK_SIZE(block);
	size_t remaining = total_size - needed_size;

	if (remaining >= MIN_BLOCK_SIZE + sizeof(footer_t) + ALIGNMENT) {
		/* Resize current block */
		SET_ALLOCATED(block, needed_size);
		GET_FOOTER(block)->size = needed_size;

		/* Create new free block from remainder */
		block_t *new_block = NEXT_BLOCK(block);
		SET_FREE(new_block, remaining);
		new_block->magic = BLOCK_MAGIC;
		new_block->next = new_block->prev = NULL;
		GET_FOOTER(new_block)->size = remaining;

		/* Add to free list */
		add_to_free_list(new_block);
	}
}

static arena_t *
create_arena(size_t min_size)
{
	size_t arena_size = (min_size < ARENA_SIZE) ? ARENA_SIZE : align_up(min_size, PAGE_SIZE);
	
	void *mem = USER_MMAP(NULL, arena_size, PROT_READ | PROT_WRITE,
	                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (mem == MAP_FAILED || mem == NULL) {
		errno = ENOMEM;
		return NULL;
	}

	arena_t *arena = (arena_t *)mem;
	arena->base = mem;
	arena->size = arena_size;
	arena->used = align_up(sizeof(arena_t), ALIGNMENT);
	arena->free_bytes = arena_size - arena->used;

	arena->next = arena_list;
	arena->prev = NULL;
	if (arena_list) {
		arena_list->prev = arena;
	}
	arena_list = arena;

	block_t *block = (block_t *)((char *)mem + arena->used);
	size_t block_size = arena_size - arena->used;
	SET_FREE(block, block_size);
	block->magic = BLOCK_MAGIC;
	block->next = block->prev = NULL;
	GET_FOOTER(block)->size = block_size;

	add_to_free_list(block);

	return arena;
}

static void *
alloc_from_free_list(size_t size)
{
	size_t needed = align_up(size + sizeof(block_t) + sizeof(footer_t), ALIGNMENT);
	int sc = get_size_class(size);

	block_t *block = NULL;

	/* Try size class bin first */
	if (sc >= 0) {
		block = size_class_bins[sc];
		if (block) {
			remove_from_free_list(block);
		}
	}

	/* Try general free list (first fit) */
	if (!block) {
		block_t *curr = general_free_list;
		while (curr) {
			if (BLOCK_SIZE(curr) >= needed) {
				block = curr;
				remove_from_free_list(block);
				break;
			}
			curr = curr->next;
		}
	}

	if (block) {
		split_block(block, needed);
		SET_ALLOCATED(block, needed);
		GET_FOOTER(block)->size = needed;
		return (void *)(block + 1);
	}

	return NULL;
}

static void *
large_alloc(size_t size)
{
	size_t total = align_up(size + sizeof(block_t), PAGE_SIZE);
	
	void *mem = USER_MMAP(NULL, total, PROT_READ | PROT_WRITE,
	                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (mem == MAP_FAILED || mem == NULL) {
		errno = ENOMEM;
		return NULL;
	}

	block_t *block = (block_t *)mem;
	SET_ALLOCATED(block, total);
	block->magic = BLOCK_MAGIC;
	block->next = (block_t *)0x1; /* Mark as large allocation */
	block->prev = NULL;

	return (void *)(block + 1);
}

void *
malloc(size_t size)
{
	if (size == 0) {
		return NULL;
	}

	lock_malloc();

	void *ptr = NULL;

	if (size >= LARGE_THRESHOLD) {
		ptr = large_alloc(size);
		unlock_malloc();
		return ptr;
	}

	ptr = alloc_from_free_list(size);

	if (!ptr) {
		if (!create_arena(size + sizeof(block_t) + sizeof(footer_t))) {
			unlock_malloc();
			errno = ENOMEM;
			return NULL;
		}
		ptr = alloc_from_free_list(size);
	}

	unlock_malloc();
	return ptr;
}

void *
calloc(size_t nmemb, size_t size)
{
	if (nmemb != 0 && size > SIZE_MAX / nmemb) {
		errno = ENOMEM;
		return NULL;
	}

	size_t total = nmemb * size;
	void *ptr = malloc(total);
	if (ptr) {
		memset(ptr, 0, total);
	}
	return ptr;
}

void
free(void *ptr)
{
	if (!ptr) {
		return;
	}

	block_t *block = (block_t *)ptr - 1;

	/* Validate */
	if (block->magic != BLOCK_MAGIC) {
		return;  /* Corruption or invalid pointer */
	}

	if (!IS_ALLOCATED(block)) {
		return;  /* Double free */
	}

	lock_malloc();

	if (block->next == (block_t *)0x1) {
		USER_MUNMAP((void *)block, BLOCK_SIZE(block));
		unlock_malloc();
		return;
	}

	size_t size = BLOCK_SIZE(block);
	SET_FREE(block, size);
	GET_FOOTER(block)->size = size;

	block = coalesce(block);
	add_to_free_list(block);

	unlock_malloc();
}

void *
realloc(void *ptr, size_t size)
{
	if (!ptr) {
		return malloc(size);
	}

	if (size == 0) {
		free(ptr);
		return NULL;
	}

	block_t *block = (block_t *)ptr - 1;
	if (block->magic != BLOCK_MAGIC) {
		errno = EINVAL;
		return NULL;
	}

	size_t old_size = BLOCK_SIZE(block) - sizeof(block_t) - sizeof(footer_t);
	
	if (old_size >= size) {
		return ptr;
	}

	void *new_ptr = malloc(size);
	if (!new_ptr) {
		return NULL;
	}

	memcpy(new_ptr, ptr, old_size);
	free(ptr);

	return new_ptr;
}

void *
reallocarray(void *ptr, size_t nmemb, size_t size)
{
	if (nmemb != 0 && size > SIZE_MAX / nmemb) {
		errno = ENOMEM;
		return NULL;
	}
	return realloc(ptr, nmemb * size);
}

void *
aligned_alloc(size_t alignment, size_t size)
{
	if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
		errno = EINVAL;
		return NULL;
	}

	if (size % alignment != 0) {
		errno = EINVAL;
		return NULL;
	}

	if (alignment <= ALIGNMENT) {
		return malloc(size);
	}

	size_t total = size + alignment;
	void *ptr = malloc(total);
	if (!ptr) {
		return NULL;
	}

	uintptr_t addr = (uintptr_t)ptr;
	uintptr_t aligned = (addr + alignment - 1) & ~(alignment - 1);

	if (aligned != addr) {
		((void **)aligned)[-1] = ptr;
	}

	return (void *)aligned;
}

int
posix_memalign(void **memptr, size_t alignment, size_t size)
{
	if (!memptr) {
		return EINVAL;
	}

	if (alignment < sizeof(void *) || (alignment & (alignment - 1)) != 0) {
		return EINVAL;
	}

	*memptr = aligned_alloc(alignment, size);
	return (*memptr == NULL) ? ENOMEM : 0;
}