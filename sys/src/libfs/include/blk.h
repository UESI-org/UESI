#ifndef FS_BLK_H
#define FS_BLK_H

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/spinlock.h>
#include <stdint.h>
#include <stdbool.h>
#include <blkalloc.h>

__BEGIN_DECLS

#define BLK_SIZE_512 512
#define BLK_SIZE_1K 1024
#define BLK_SIZE_2K 2048
#define BLK_SIZE_4K 4096
#define BLK_SIZE_8K 8192

#define BLK_SIZE_DEFAULT BLK_SIZE_4K

#define BLK_DEV_READONLY 0x0001  /* Read-only device */
#define BLK_DEV_REMOVABLE 0x0002 /* Removable media */
#define BLK_DEV_VIRTUAL 0x0004   /* Virtual device (RAM disk, etc) */

#define BLK_READ 0  /* Read operation */
#define BLK_WRITE 1 /* Write operation */
#define BLK_FLUSH 2 /* Flush cache */

struct blk_device;
struct blk_request;

typedef struct blk_ops {
	int (*read)(struct blk_device *dev,
	            uint64_t block,
	            void *buffer,
	            size_t count);
	int (*write)(struct blk_device *dev,
	             uint64_t block,
	             const void *buffer,
	             size_t count);
	int (*flush)(struct blk_device *dev);
	int (*get_info)(struct blk_device *dev, void *info);
} blk_ops_t;

typedef struct blk_device {
	dev_t bd_dev;
	const char *bd_name;
	uint32_t bd_flags;
	uint32_t bd_block_size;
	uint64_t bd_total_blocks;
	blk_ops_t *bd_ops;
	void *bd_private;
	spinlock_t bd_lock;
	struct blk_device *bd_next;
} blk_device_t;

typedef struct blk_buffer {
	int64_t bb_block;
	blk_device_t *bb_dev;
	void *bb_data;
	uint32_t bb_size;
	uint32_t bb_flags;
	uint32_t bb_refcount;
	spinlock_t bb_lock;
	struct blk_buffer *bb_next;
} blk_buffer_t;

#define BLK_BUF_DIRTY 0x0001
#define BLK_BUF_VALID 0x0002
#define BLK_BUF_LOCKED 0x0004

int blk_register_device(blk_device_t *dev);
int blk_unregister_device(dev_t dev);
blk_device_t *blk_get_device(dev_t dev);
blk_device_t *blk_find_device(const char *name);

int blk_read(blk_device_t *dev, uint64_t block, void *buffer, size_t count);
int blk_write(blk_device_t *dev,
              uint64_t block,
              const void *buffer,
              size_t count);
int blk_flush(blk_device_t *dev);
int blk_read_block(blk_device_t *dev, uint64_t block, void *buffer);
int blk_write_block(blk_device_t *dev, uint64_t block, const void *buffer);

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

int blk_mark_used(blk_allocator_t *alloc, uint32_t block);
int blk_mark_range_used(blk_allocator_t *alloc, uint32_t start, uint32_t count);

int blk_buffer_init(void);
int blk_buffer_get(blk_device_t *dev, uint64_t block, blk_buffer_t **buf);
void blk_buffer_put(blk_buffer_t *buf);
void blk_buffer_mark_dirty(blk_buffer_t *buf);
int blk_buffer_sync(blk_buffer_t *buf);
int blk_buffer_sync_device(blk_device_t *dev);
int blk_buffer_sync_all(void);
void blk_buffer_invalidate_device(blk_device_t *dev);

blk_device_t *blk_ramdisk_create(const char *name, size_t size_bytes);
int blk_ramdisk_destroy(blk_device_t *dev);

static inline uint64_t
blk_bytes_to_blocks(uint64_t bytes, uint32_t block_size)
{
	return (bytes + block_size - 1) / block_size;
}
static inline uint64_t
blk_blocks_to_bytes(uint64_t blocks, uint32_t block_size)
{
	return blocks * block_size;
}
static inline bool
blk_is_valid_block(blk_device_t *dev, uint64_t block)
{
	return block < dev->bd_total_blocks;
}

void blk_dump_devices(void);
void blk_dump_allocator(blk_allocator_t *alloc);

__END_DECLS

#endif
