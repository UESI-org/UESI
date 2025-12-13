#ifndef _FS_BLKALLOC_H_
#define _FS_BLKALLOC_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/spinlock.h>

__BEGIN_DECLS

struct vfs_mount;

typedef struct blk_allocator {
	uint32_t ba_total_blocks;     /* Total number of blocks */
	uint32_t ba_used_blocks;      /* Number of allocated blocks */
	uint32_t ba_first_data_block; /* First usable data block */
	uint32_t ba_first_free;       /* Hint: first potentially free block */
	uint32_t ba_block_size;       /* Block size in bytes */

	uint8_t *ba_bitmap;      /* Bitmap of block allocation status */
	uint32_t ba_bitmap_size; /* Size of bitmap in bytes */

	spinlock_t ba_lock; /* Synchronization lock */

	struct vfs_mount *ba_mount; /* Associated mount point */
	uint32_t ba_bitmap_block; /* Block number where bitmap starts on disk */
	uint32_t ba_bitmap_blocks; /* Number of blocks occupied by bitmap */

	bool ba_dirty; /* Bitmap needs to be written to disk */
} blk_allocator_t;

typedef struct blk_alloc_stats {
	uint32_t total_blocks;  /* Total blocks in filesystem */
	uint32_t used_blocks;   /* Blocks currently allocated */
	uint32_t free_blocks;   /* Blocks available for allocation */
	uint32_t first_free;    /* Hint for next allocation */
	uint32_t fragmentation; /* Percentage (0-100) */
} blk_alloc_stats_t;

int blk_alloc_init(blk_allocator_t *alloc,
                   uint32_t total_blocks,
                   uint32_t first_data_block,
                   uint32_t block_size);

void blk_alloc_destroy(blk_allocator_t *alloc);
uint32_t blk_alloc(blk_allocator_t *alloc);
int blk_alloc_specific(blk_allocator_t *alloc, uint32_t block_num);
uint32_t blk_alloc_contiguous(blk_allocator_t *alloc,
                              uint32_t count,
                              uint32_t *blocks);

int blk_free(blk_allocator_t *alloc, uint32_t block_num);
uint32_t blk_free_multiple(blk_allocator_t *alloc,
                           uint32_t *blocks,
                           uint32_t count);

bool blk_is_allocated(blk_allocator_t *alloc, uint32_t block_num);

void blk_alloc_get_stats(blk_allocator_t *alloc, blk_alloc_stats_t *stats);
void blk_alloc_mark_dirty(blk_allocator_t *alloc);
int blk_alloc_sync(blk_allocator_t *alloc);
int blk_alloc_load(blk_allocator_t *alloc,
                   struct vfs_mount *mount,
                   uint32_t bitmap_block);

int blk_alloc_format(blk_allocator_t *alloc, uint32_t reserved_blocks);
uint32_t blk_alloc_fragmentation(blk_allocator_t *alloc);
uint32_t blk_alloc_largest_free_region(blk_allocator_t *alloc,
                                       uint32_t *start_block);
void blk_alloc_debug_print(blk_allocator_t *alloc);
int blk_alloc_validate(blk_allocator_t *alloc);

__END_DECLS

#endif