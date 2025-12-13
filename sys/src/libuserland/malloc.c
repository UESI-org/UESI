#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <errno.h>

#ifdef _KERNEL
extern int64_t sys_mmap(
    void *addr, size_t length, int prot, int flags, int fd, off_t offset);
extern int64_t sys_munmap(void *addr, size_t length);

static inline void *
kernel_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
	int64_t ret = sys_mmap(addr, len, prot, flags, fd, off);
	if (ret < 0) {
		return MAP_FAILED;
	}
	return (void *)ret;
}

static inline int
kernel_munmap(void *addr, size_t len)
{
	return (int)sys_munmap(addr, len);
}

#define USER_MMAP(addr, len, prot, flags, fd, off)                             \
	kernel_mmap(addr, len, prot, flags, fd, off)
#define USER_MUNMAP(addr, len) kernel_munmap(addr, len)
#else

extern void *mmap(
    void *addr, size_t length, int prot, int flags, int fd, off_t offset);
extern int munmap(void *addr, size_t length);

#define USER_MMAP mmap
#define USER_MUNMAP munmap
#endif

#define PAGE_SIZE 4096
#define MIN_ALLOC_SIZE 16
#define ALIGNMENT 16
#define LARGE_THRESHOLD (PAGE_SIZE / 2)

typedef struct block_header {
	size_t size;               /* Size of usable data */
	struct block_header *next; /* Next free block */
	uint32_t magic;            /* Magic number for validation */
	uint32_t flags;            /* Flags (allocated/free) */
} block_header_t;

#define BLOCK_MAGIC 0xDEADBEEF
#define BLOCK_FREE 0x00000001
#define BLOCK_ALLOCATED 0x00000002
#define BLOCK_LARGE 0x00000004

typedef struct arena {
	void *base;
	size_t size;
	size_t used;
	struct arena *next;
} arena_t;

static block_header_t *free_list = NULL;
static arena_t *arena_list = NULL;

static volatile int malloc_lock = 0;

static inline void
lock_malloc(void)
{
	while (__sync_lock_test_and_set(&malloc_lock, 1)) {
		/* Spin */
	}
}

static inline void
unlock_malloc(void)
{
	__sync_lock_release(&malloc_lock);
}

static inline size_t
align_size(size_t size)
{
	return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

static arena_t *
create_arena(size_t min_size)
{
	size_t arena_size =
	    (min_size < PAGE_SIZE * 16) ? (PAGE_SIZE * 16) : min_size;
	arena_size = (arena_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

	void *mem = USER_MMAP(NULL,
	                      arena_size,
	                      PROT_READ | PROT_WRITE,
	                      MAP_PRIVATE | MAP_ANONYMOUS,
	                      -1,
	                      0);

	if (mem == MAP_FAILED || mem == NULL) {
		return NULL;
	}

	arena_t *arena = (arena_t *)mem;
	arena->base = mem;
	arena->size = arena_size;
	arena->used = sizeof(arena_t);
	arena->next = arena_list;
	arena_list = arena;

	return arena;
}

static void *
arena_alloc(size_t size)
{
	size = align_size(size + sizeof(block_header_t));

	/* Try existing arenas */
	for (arena_t *arena = arena_list; arena != NULL; arena = arena->next) {
		if (arena->used + size <= arena->size) {
			void *ptr = (char *)arena->base + arena->used;
			block_header_t *header = (block_header_t *)ptr;
			header->size = size - sizeof(block_header_t);
			header->next = NULL;
			header->magic = BLOCK_MAGIC;
			header->flags = BLOCK_ALLOCATED;
			arena->used += size;
			return (void *)(header + 1);
		}
	}

	/* Create new arena */
	arena_t *arena = create_arena(size);
	if (!arena) {
		return NULL;
	}

	void *ptr = (char *)arena->base + arena->used;
	block_header_t *header = (block_header_t *)ptr;
	header->size = size - sizeof(block_header_t);
	header->next = NULL;
	header->magic = BLOCK_MAGIC;
	header->flags = BLOCK_ALLOCATED;
	arena->used += size;

	return (void *)(header + 1);
}

static void *
large_alloc(size_t size)
{
	size_t total_size = size + sizeof(block_header_t);
	total_size = (total_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

	void *mem = USER_MMAP(NULL,
	                      total_size,
	                      PROT_READ | PROT_WRITE,
	                      MAP_PRIVATE | MAP_ANONYMOUS,
	                      -1,
	                      0);

	if (mem == MAP_FAILED || mem == NULL) {
		return NULL;
	}

	block_header_t *header = (block_header_t *)mem;
	header->size = total_size - sizeof(block_header_t);
	header->next = NULL;
	header->magic = BLOCK_MAGIC;
	header->flags = BLOCK_ALLOCATED | BLOCK_LARGE;

	return (void *)(header + 1);
}

static void
large_free(block_header_t *header)
{
	size_t total_size = header->size + sizeof(block_header_t);
	USER_MUNMAP((void *)header, total_size);
}

void *
malloc(size_t size)
{
	if (size == 0) {
		return NULL;
	}

	lock_malloc();

	void *ptr;
	if (size >= LARGE_THRESHOLD) {
		ptr = large_alloc(size);
	} else {
		/* Try free list first */
		block_header_t **prev = &free_list;
		block_header_t *block = free_list;

		while (block != NULL) {
			if (block->size >= size) {
				/* Found suitable block */
				*prev = block->next;
				block->flags = BLOCK_ALLOCATED;
				block->next = NULL;
				unlock_malloc();
				return (void *)(block + 1);
			}
			prev = &block->next;
			block = block->next;
		}

		ptr = arena_alloc(size);
	}

	unlock_malloc();
	return ptr;
}

void *
calloc(size_t nmemb, size_t size)
{
	/* Check for overflow */
	if (nmemb != 0 && size > SIZE_MAX / nmemb) {
		errno = ENOMEM;
		return NULL;
	}

	size_t total = nmemb * size;
	void *ptr = malloc(total);

	if (ptr != NULL) {
		memset(ptr, 0, total);
	}

	return ptr;
}

void
free(void *ptr)
{
	if (ptr == NULL) {
		return;
	}

	block_header_t *header = (block_header_t *)ptr - 1;

	if (header->magic != BLOCK_MAGIC) {
		return;
	}

	if (header->flags & BLOCK_FREE) {
		return;
	}

	lock_malloc();

	if (header->flags & BLOCK_LARGE) {
		large_free(header);
	} else {
		header->flags = BLOCK_FREE;
		header->next = free_list;
		free_list = header;
	}

	unlock_malloc();
}

void *
realloc(void *ptr, size_t size)
{
	if (ptr == NULL) {
		return malloc(size);
	}

	if (size == 0) {
		free(ptr);
		return NULL;
	}

	block_header_t *header = (block_header_t *)ptr - 1;

	if (header->magic != BLOCK_MAGIC) {
		return NULL;
	}

	if (header->size >= size) {
		return ptr;
	}

	void *new_ptr = malloc(size);
	if (new_ptr == NULL) {
		return NULL;
	}

	size_t copy_size = (header->size < size) ? header->size : size;
	memcpy(new_ptr, ptr, copy_size);

	free(ptr);

	return new_ptr;
}

void *
reallocarray(void *ptr, size_t nmemb, size_t size)
{
	/* Check for overflow */
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

	size_t total_size = size + alignment + sizeof(block_header_t);
	void *ptr = malloc(total_size);

	if (ptr == NULL) {
		return NULL;
	}

	uintptr_t addr = (uintptr_t)ptr;
	uintptr_t aligned = (addr + alignment - 1) & ~(alignment - 1);

	if (aligned == addr) {
		return ptr;
	}

	size_t offset = aligned - addr;
	*((size_t *)(aligned - sizeof(size_t))) = offset;

	return (void *)aligned;
}

int
posix_memalign(void **memptr, size_t alignment, size_t size)
{
	if (memptr == NULL) {
		return EINVAL;
	}

	if (alignment < sizeof(void *) || (alignment & (alignment - 1)) != 0) {
		return EINVAL;
	}

	void *ptr = aligned_alloc(alignment, size);
	if (ptr == NULL) {
		return ENOMEM;
	}

	*memptr = ptr;
	return 0;
}