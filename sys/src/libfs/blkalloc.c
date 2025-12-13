/* Complex software, keep heavily commented !!! */

#include <blkalloc.h>
#include <vfs.h>
#include <errno.h>
#include <kmalloc.h>
#include <kfree.h>
#include <string.h>
#include <sys/panic.h>

extern void serial_printf(uint16_t port, const char *fmt, ...);
#define DEBUG_PORT 0x3F8
#define BLKALLOC_DEBUG(fmt, ...)                                               \
	serial_printf(DEBUG_PORT, "[BLKALLOC] " fmt, ##__VA_ARGS__)

static inline bool
bitmap_test_bit(uint8_t *bitmap, uint32_t bit)
{
	uint32_t byte = bit / 8;
	uint32_t bit_pos = bit % 8;
	return (bitmap[byte] & (1 << bit_pos)) != 0;
}

static inline void
bitmap_set_bit(uint8_t *bitmap, uint32_t bit)
{
	uint32_t byte = bit / 8;
	uint32_t bit_pos = bit % 8;
	bitmap[byte] |= (1 << bit_pos);
}

static inline void
bitmap_clear_bit(uint8_t *bitmap, uint32_t bit)
{
	uint32_t byte = bit / 8;
	uint32_t bit_pos = bit % 8;
	bitmap[byte] &= ~(1 << bit_pos);
}

static inline uint32_t
find_first_zero_bit_in_byte(uint8_t byte)
{
	for (uint32_t i = 0; i < 8; i++) {
		if ((byte & (1 << i)) == 0) {
			return i;
		}
	}
	return 8;
}

int
blk_alloc_init(blk_allocator_t *alloc,
               uint32_t total_blocks,
               uint32_t first_data_block,
               uint32_t block_size)
{
	if (alloc == NULL || total_blocks == 0 || block_size == 0) {
		return -EINVAL;
	}

	if (first_data_block >= total_blocks) {
		return -EINVAL;
	}

	/* Calculate bitmap size in bytes (1 bit per block) */
	uint32_t bitmap_size = (total_blocks + 7) / 8;

	/* Allocate bitmap in memory */
	alloc->ba_bitmap = kmalloc(bitmap_size);
	if (alloc->ba_bitmap == NULL) {
		BLKALLOC_DEBUG("Failed to allocate bitmap (%u bytes)\n",
		               bitmap_size);
		return -ENOMEM;
	}

	/* Initialize all blocks as free */
	memset(alloc->ba_bitmap, 0, bitmap_size);

	/* Initialize allocator structure */
	alloc->ba_total_blocks = total_blocks;
	alloc->ba_used_blocks = 0;
	alloc->ba_first_data_block = first_data_block;
	alloc->ba_first_free = first_data_block;
	alloc->ba_block_size = block_size;
	alloc->ba_bitmap_size = bitmap_size;
	alloc->ba_mount = NULL;
	alloc->ba_bitmap_block = 0;
	alloc->ba_dirty = false;

	/* Calculate how many blocks the bitmap occupies on disk */
	alloc->ba_bitmap_blocks = (bitmap_size + block_size - 1) / block_size;

	spinlock_init(&alloc->ba_lock, "blk_alloc");

	BLKALLOC_DEBUG(
	    "Initialized: %u blocks, bitmap size %u bytes (%u blocks)\n",
	    total_blocks,
	    bitmap_size,
	    alloc->ba_bitmap_blocks);

	return 0;
}

void
blk_alloc_destroy(blk_allocator_t *alloc)
{
	if (alloc == NULL) {
		return;
	}

	/* Sync any pending changes */
	if (alloc->ba_dirty && alloc->ba_mount != NULL) {
		BLKALLOC_DEBUG(
		    "Warning: destroying dirty allocator, syncing first\n");
		blk_alloc_sync(alloc);
	}

	/* Free bitmap memory */
	if (alloc->ba_bitmap != NULL) {
		kfree(alloc->ba_bitmap);
		alloc->ba_bitmap = NULL;
	}

	BLKALLOC_DEBUG("Destroyed allocator\n");
}

uint32_t
blk_alloc(blk_allocator_t *alloc)
{
	if (alloc == NULL) {
		return 0;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&alloc->ba_lock, &flags);

	/* Check if filesystem is full */
	if (alloc->ba_used_blocks >= alloc->ba_total_blocks) {
		spinlock_release_irqrestore(&alloc->ba_lock, flags);
		BLKALLOC_DEBUG("Filesystem full: %u/%u blocks used\n",
		               alloc->ba_used_blocks,
		               alloc->ba_total_blocks);
		return 0;
	}

	/* Search for free block starting from hint */
	uint32_t start = alloc->ba_first_free;
	uint32_t block;

	/* Search from hint to end */
	for (block = start; block < alloc->ba_total_blocks; block++) {
		if (!bitmap_test_bit(alloc->ba_bitmap, block)) {
			/* Found free block */
			bitmap_set_bit(alloc->ba_bitmap, block);
			alloc->ba_used_blocks++;
			alloc->ba_first_free = block + 1;
			alloc->ba_dirty = true;

			spinlock_release_irqrestore(&alloc->ba_lock, flags);

			BLKALLOC_DEBUG("Allocated block %u (%u/%u used)\n",
			               block,
			               alloc->ba_used_blocks,
			               alloc->ba_total_blocks);
			return block;
		}
	}

	/* Wrap around and search from first_data_block to hint */
	for (block = alloc->ba_first_data_block; block < start; block++) {
		if (!bitmap_test_bit(alloc->ba_bitmap, block)) {
			bitmap_set_bit(alloc->ba_bitmap, block);
			alloc->ba_used_blocks++;
			alloc->ba_first_free = block + 1;
			alloc->ba_dirty = true;

			spinlock_release_irqrestore(&alloc->ba_lock, flags);

			BLKALLOC_DEBUG(
			    "Allocated block %u (wrapped, %u/%u used)\n",
			    block,
			    alloc->ba_used_blocks,
			    alloc->ba_total_blocks);
			return block;
		}
	}

	/* No free blocks found (shouldn't happen given the check above) */
	spinlock_release_irqrestore(&alloc->ba_lock, flags);
	BLKALLOC_DEBUG("No free blocks found (inconsistency?)\n");
	return 0;
}

int
blk_alloc_specific(blk_allocator_t *alloc, uint32_t block_num)
{
	if (alloc == NULL) {
		return -EINVAL;
	}

	if (block_num >= alloc->ba_total_blocks) {
		return -EINVAL;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&alloc->ba_lock, &flags);

	/* Check if already allocated */
	if (bitmap_test_bit(alloc->ba_bitmap, block_num)) {
		spinlock_release_irqrestore(&alloc->ba_lock, flags);
		return -EEXIST;
	}

	/* Allocate the block */
	bitmap_set_bit(alloc->ba_bitmap, block_num);
	alloc->ba_used_blocks++;
	alloc->ba_dirty = true;

	/* Update hint if necessary */
	if (block_num < alloc->ba_first_free) {
		alloc->ba_first_free = block_num + 1;
	}

	spinlock_release_irqrestore(&alloc->ba_lock, flags);

	BLKALLOC_DEBUG("Allocated specific block %u\n", block_num);
	return 0;
}

uint32_t
blk_alloc_contiguous(blk_allocator_t *alloc, uint32_t count, uint32_t *blocks)
{
	if (alloc == NULL || count == 0 || blocks == NULL) {
		return 0;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&alloc->ba_lock, &flags);

	/* Check available space */
	uint32_t available = alloc->ba_total_blocks - alloc->ba_used_blocks;
	if (available == 0) {
		spinlock_release_irqrestore(&alloc->ba_lock, flags);
		return 0;
	}

	/* Limit to available blocks */
	if (count > available) {
		count = available;
	}

	/* Search for contiguous region */
	uint32_t found = 0;
	uint32_t start_block = 0;

	for (uint32_t block = alloc->ba_first_data_block;
	     block < alloc->ba_total_blocks && found < count;
	     block++) {
		if (!bitmap_test_bit(alloc->ba_bitmap, block)) {
			if (found == 0) {
				start_block = block;
			}
			found++;
		} else {
			/* Reset if we hit an allocated block */
			found = 0;
		}
	}

	/* If we didn't find enough contiguous blocks, take what we found */
	if (found == 0) {
		spinlock_release_irqrestore(&alloc->ba_lock, flags);
		return 0;
	}

	/* Allocate the blocks we found */
	for (uint32_t i = 0; i < found; i++) {
		uint32_t block = start_block + i;
		bitmap_set_bit(alloc->ba_bitmap, block);
		blocks[i] = block;
	}

	alloc->ba_used_blocks += found;
	alloc->ba_first_free = start_block + found;
	alloc->ba_dirty = true;

	spinlock_release_irqrestore(&alloc->ba_lock, flags);

	BLKALLOC_DEBUG("Allocated %u contiguous blocks starting at %u\n",
	               found,
	               start_block);
	return found;
}

int
blk_free(blk_allocator_t *alloc, uint32_t block_num)
{
	if (alloc == NULL) {
		return -EINVAL;
	}

	if (block_num >= alloc->ba_total_blocks) {
		return -EINVAL;
	}

	/* Don't allow freeing reserved blocks */
	if (block_num < alloc->ba_first_data_block) {
		BLKALLOC_DEBUG("Attempt to free reserved block %u\n",
		               block_num);
		return -EINVAL;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&alloc->ba_lock, &flags);

	/* Check if already free */
	if (!bitmap_test_bit(alloc->ba_bitmap, block_num)) {
		spinlock_release_irqrestore(&alloc->ba_lock, flags);
		BLKALLOC_DEBUG("Double free attempt on block %u\n", block_num);
		return -EINVAL;
	}

	/* Free the block */
	bitmap_clear_bit(alloc->ba_bitmap, block_num);
	alloc->ba_used_blocks--;
	alloc->ba_dirty = true;

	/* Update hint if this is before current hint */
	if (block_num < alloc->ba_first_free) {
		alloc->ba_first_free = block_num;
	}

	spinlock_release_irqrestore(&alloc->ba_lock, flags);

	BLKALLOC_DEBUG("Freed block %u (%u/%u used)\n",
	               block_num,
	               alloc->ba_used_blocks,
	               alloc->ba_total_blocks);
	return 0;
}

uint32_t
blk_free_multiple(blk_allocator_t *alloc, uint32_t *blocks, uint32_t count)
{
	if (alloc == NULL || blocks == NULL || count == 0) {
		return 0;
	}

	uint32_t freed = 0;

	for (uint32_t i = 0; i < count; i++) {
		if (blk_free(alloc, blocks[i]) == 0) {
			freed++;
		}
	}

	BLKALLOC_DEBUG("Freed %u/%u blocks\n", freed, count);
	return freed;
}

bool
blk_is_allocated(blk_allocator_t *alloc, uint32_t block_num)
{
	if (alloc == NULL || block_num >= alloc->ba_total_blocks) {
		return false;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&alloc->ba_lock, &flags);
	bool allocated = bitmap_test_bit(alloc->ba_bitmap, block_num);
	spinlock_release_irqrestore(&alloc->ba_lock, flags);

	return allocated;
}

void
blk_alloc_get_stats(blk_allocator_t *alloc, blk_alloc_stats_t *stats)
{
	if (alloc == NULL || stats == NULL) {
		return;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&alloc->ba_lock, &flags);

	stats->total_blocks = alloc->ba_total_blocks;
	stats->used_blocks = alloc->ba_used_blocks;
	stats->free_blocks = alloc->ba_total_blocks - alloc->ba_used_blocks;
	stats->first_free = alloc->ba_first_free;
	stats->fragmentation = blk_alloc_fragmentation(alloc);

	spinlock_release_irqrestore(&alloc->ba_lock, flags);
}

void
blk_alloc_mark_dirty(blk_allocator_t *alloc)
{
	if (alloc == NULL) {
		return;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&alloc->ba_lock, &flags);
	alloc->ba_dirty = true;
	spinlock_release_irqrestore(&alloc->ba_lock, flags);
}

int
blk_alloc_sync(blk_allocator_t *alloc)
{
	if (alloc == NULL) {
		return -EINVAL;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&alloc->ba_lock, &flags);

	if (!alloc->ba_dirty) {
		spinlock_release_irqrestore(&alloc->ba_lock, flags);
		return 0; /* Nothing to sync */
	}

	if (alloc->ba_mount == NULL) {
		spinlock_release_irqrestore(&alloc->ba_lock, flags);
		BLKALLOC_DEBUG("Cannot sync: no mount point\n");
		return -EINVAL;
	}

	/* Get device path from mount */
	const char *device = alloc->ba_mount->mnt_device;
	if (device == NULL) {
		spinlock_release_irqrestore(&alloc->ba_lock, flags);
		BLKALLOC_DEBUG("Cannot sync: no device\n");
		return -EINVAL;
	}

	spinlock_release_irqrestore(&alloc->ba_lock, flags);

	/* Open device for writing */
	vfs_file_t *dev_file;
	int ret = vfs_open(device, VFS_O_RDWR, 0, &dev_file);
	if (ret != 0) {
		BLKALLOC_DEBUG("Failed to open device %s: %d\n", device, ret);
		return ret;
	}

	/* Write bitmap blocks to disk */
	for (uint32_t i = 0; i < alloc->ba_bitmap_blocks; i++) {
		uint32_t block_num = alloc->ba_bitmap_block + i;
		uint32_t offset = i * alloc->ba_block_size;
		uint32_t size = alloc->ba_block_size;

		/* Handle last block (may be partial) */
		if (i == alloc->ba_bitmap_blocks - 1) {
			uint32_t remaining = alloc->ba_bitmap_size - offset;
			if (remaining < size) {
				size = remaining;
			}
		}

		/* Calculate disk offset */
		off_t disk_offset = (off_t)block_num * alloc->ba_block_size;

		/* Seek to block position */
		off_t seek_result =
		    vfs_seek(dev_file, disk_offset, VFS_SEEK_SET);
		if (seek_result < 0) {
			BLKALLOC_DEBUG("Seek failed for block %u: %ld\n",
			               block_num,
			               seek_result);
			vfs_close(dev_file);
			return seek_result;
		}

		/* Write bitmap data */
		ssize_t written =
		    vfs_write(dev_file, alloc->ba_bitmap + offset, size);
		if (written != (ssize_t)size) {
			BLKALLOC_DEBUG(
			    "Write failed for block %u: wrote %ld/%u bytes\n",
			    block_num,
			    written,
			    size);
			vfs_close(dev_file);
			return written < 0 ? written : -EIO;
		}

		BLKALLOC_DEBUG(
		    "Wrote bitmap block %u/%u (%u bytes at offset %lu)\n",
		    i + 1,
		    alloc->ba_bitmap_blocks,
		    size,
		    disk_offset);
	}

	/* Sync to ensure data reaches disk */
	ret = vfs_fsync(dev_file);
	if (ret != 0) {
		BLKALLOC_DEBUG("fsync failed: %d\n", ret);
		vfs_close(dev_file);
		return ret;
	}

	/* Close device */
	vfs_close(dev_file);

	/* Mark as clean */
	spinlock_acquire_irqsave(&alloc->ba_lock, &flags);
	alloc->ba_dirty = false;
	spinlock_release_irqrestore(&alloc->ba_lock, flags);

	BLKALLOC_DEBUG("Synced bitmap to disk (%u blocks, %u bytes)\n",
	               alloc->ba_bitmap_blocks,
	               alloc->ba_bitmap_size);
	return 0;
}

int
blk_alloc_load(blk_allocator_t *alloc,
               struct vfs_mount *mount,
               uint32_t bitmap_block)
{
	if (alloc == NULL || mount == NULL) {
		return -EINVAL;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&alloc->ba_lock, &flags);

	alloc->ba_mount = mount;
	alloc->ba_bitmap_block = bitmap_block;

	spinlock_release_irqrestore(&alloc->ba_lock, flags);

	/* Get device path from mount */
	const char *device = mount->mnt_device;
	if (device == NULL) {
		BLKALLOC_DEBUG("Cannot load: no device\n");
		return -EINVAL;
	}

	/* Open device for reading */
	vfs_file_t *dev_file;
	int ret = vfs_open(device, VFS_O_RDONLY, 0, &dev_file);
	if (ret != 0) {
		BLKALLOC_DEBUG("Failed to open device %s: %d\n", device, ret);
		return ret;
	}

	/* Read bitmap blocks from disk */
	for (uint32_t i = 0; i < alloc->ba_bitmap_blocks; i++) {
		uint32_t block_num = bitmap_block + i;
		uint32_t offset = i * alloc->ba_block_size;
		uint32_t size = alloc->ba_block_size;

		/* Handle last block (may be partial) */
		if (i == alloc->ba_bitmap_blocks - 1) {
			uint32_t remaining = alloc->ba_bitmap_size - offset;
			if (remaining < size) {
				size = remaining;
			}
		}

		/* Calculate disk offset */
		off_t disk_offset = (off_t)block_num * alloc->ba_block_size;

		/* Seek to block position */
		off_t seek_result =
		    vfs_seek(dev_file, disk_offset, VFS_SEEK_SET);
		if (seek_result < 0) {
			BLKALLOC_DEBUG("Seek failed for block %u: %ld\n",
			               block_num,
			               seek_result);
			vfs_close(dev_file);
			return seek_result;
		}

		/* Read bitmap data */
		ssize_t bytes_read =
		    vfs_read(dev_file, alloc->ba_bitmap + offset, size);
		if (bytes_read != (ssize_t)size) {
			BLKALLOC_DEBUG(
			    "Read failed for block %u: read %ld/%u bytes\n",
			    block_num,
			    bytes_read,
			    size);
			vfs_close(dev_file);
			return bytes_read < 0 ? bytes_read : -EIO;
		}

		BLKALLOC_DEBUG(
		    "Read bitmap block %u/%u (%u bytes from offset %lu)\n",
		    i + 1,
		    alloc->ba_bitmap_blocks,
		    size,
		    disk_offset);
	}

	/* Close device */
	vfs_close(dev_file);

	/* Recalculate used blocks count from bitmap */
	spinlock_acquire_irqsave(&alloc->ba_lock, &flags);

	alloc->ba_used_blocks = 0;
	for (uint32_t i = 0; i < alloc->ba_total_blocks; i++) {
		if (bitmap_test_bit(alloc->ba_bitmap, i)) {
			alloc->ba_used_blocks++;
		}
	}

	/* Find first free block hint */
	alloc->ba_first_free = alloc->ba_first_data_block;
	for (uint32_t i = alloc->ba_first_data_block;
	     i < alloc->ba_total_blocks;
	     i++) {
		if (!bitmap_test_bit(alloc->ba_bitmap, i)) {
			alloc->ba_first_free = i;
			break;
		}
	}

	alloc->ba_dirty = false;
	spinlock_release_irqrestore(&alloc->ba_lock, flags);

	BLKALLOC_DEBUG(
	    "Loaded bitmap from disk (block %u): %u/%u blocks used\n",
	    bitmap_block,
	    alloc->ba_used_blocks,
	    alloc->ba_total_blocks);
	return 0;
}

int
blk_alloc_format(blk_allocator_t *alloc, uint32_t reserved_blocks)
{
	if (alloc == NULL) {
		return -EINVAL;
	}

	if (reserved_blocks > alloc->ba_total_blocks) {
		return -EINVAL;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&alloc->ba_lock, &flags);

	/* Clear entire bitmap first */
	memset(alloc->ba_bitmap, 0, alloc->ba_bitmap_size);
	alloc->ba_used_blocks = 0;

	/* Mark reserved blocks as allocated */
	for (uint32_t i = 0; i < reserved_blocks; i++) {
		bitmap_set_bit(alloc->ba_bitmap, i);
		alloc->ba_used_blocks++;
	}

	alloc->ba_first_free = reserved_blocks;
	alloc->ba_dirty = true;

	spinlock_release_irqrestore(&alloc->ba_lock, flags);

	BLKALLOC_DEBUG("Formatted: %u reserved blocks, %u data blocks\n",
	               reserved_blocks,
	               alloc->ba_total_blocks - reserved_blocks);
	return 0;
}

uint32_t
blk_alloc_fragmentation(blk_allocator_t *alloc)
{
	if (alloc == NULL || alloc->ba_used_blocks == 0) {
		return 0;
	}

	/* Count number of free->allocated transitions (fragments) */
	uint32_t fragments = 0;
	bool in_free_region = false;

	for (uint32_t i = alloc->ba_first_data_block;
	     i < alloc->ba_total_blocks;
	     i++) {
		bool is_free = !bitmap_test_bit(alloc->ba_bitmap, i);

		if (is_free && !in_free_region) {
			in_free_region = true;
		} else if (!is_free && in_free_region) {
			fragments++;
			in_free_region = false;
		}
	}

	/* Calculate fragmentation as percentage */
	/* More fragments = more fragmented */
	uint32_t max_fragments = alloc->ba_used_blocks;
	if (max_fragments == 0) {
		return 0;
	}

	return (fragments * 100) / max_fragments;
}

uint32_t
blk_alloc_largest_free_region(blk_allocator_t *alloc, uint32_t *start_block)
{
	if (alloc == NULL) {
		return 0;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&alloc->ba_lock, &flags);

	uint32_t max_size = 0;
	uint32_t max_start = 0;
	uint32_t current_size = 0;
	uint32_t current_start = 0;

	for (uint32_t i = alloc->ba_first_data_block;
	     i < alloc->ba_total_blocks;
	     i++) {
		if (!bitmap_test_bit(alloc->ba_bitmap, i)) {
			if (current_size == 0) {
				current_start = i;
			}
			current_size++;
		} else {
			if (current_size > max_size) {
				max_size = current_size;
				max_start = current_start;
			}
			current_size = 0;
		}
	}

	/* Check final region */
	if (current_size > max_size) {
		max_size = current_size;
		max_start = current_start;
	}

	spinlock_release_irqrestore(&alloc->ba_lock, flags);

	if (start_block != NULL) {
		*start_block = max_start;
	}

	return max_size;
}

void
blk_alloc_debug_print(blk_allocator_t *alloc)
{
	if (alloc == NULL) {
		return;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&alloc->ba_lock, &flags);

	BLKALLOC_DEBUG("=== Block Allocator State ===\n");
	BLKALLOC_DEBUG("  Total blocks:      %u\n", alloc->ba_total_blocks);
	BLKALLOC_DEBUG("  Used blocks:       %u\n", alloc->ba_used_blocks);
	BLKALLOC_DEBUG("  Free blocks:       %u\n",
	               alloc->ba_total_blocks - alloc->ba_used_blocks);
	BLKALLOC_DEBUG("  First data block:  %u\n", alloc->ba_first_data_block);
	BLKALLOC_DEBUG("  First free hint:   %u\n", alloc->ba_first_free);
	BLKALLOC_DEBUG("  Block size:        %u bytes\n", alloc->ba_block_size);
	BLKALLOC_DEBUG("  Bitmap size:       %u bytes (%u blocks)\n",
	               alloc->ba_bitmap_size,
	               alloc->ba_bitmap_blocks);
	BLKALLOC_DEBUG("  Dirty:             %s\n",
	               alloc->ba_dirty ? "yes" : "no");
	BLKALLOC_DEBUG("  Fragmentation:     %u%%\n",
	               blk_alloc_fragmentation(alloc));

	uint32_t largest_start;
	uint32_t largest_size =
	    blk_alloc_largest_free_region(alloc, &largest_start);
	BLKALLOC_DEBUG("  Largest free:      %u blocks at %u\n",
	               largest_size,
	               largest_start);

	spinlock_release_irqrestore(&alloc->ba_lock, flags);
	BLKALLOC_DEBUG("=============================\n");
}

int
blk_alloc_validate(blk_allocator_t *alloc)
{
	if (alloc == NULL) {
		return -EINVAL;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&alloc->ba_lock, &flags);

	/* Count actual used blocks and compare with stored count */
	uint32_t actual_used = 0;
	for (uint32_t i = 0; i < alloc->ba_total_blocks; i++) {
		if (bitmap_test_bit(alloc->ba_bitmap, i)) {
			actual_used++;
		}
	}

	if (actual_used != alloc->ba_used_blocks) {
		BLKALLOC_DEBUG(
		    "CORRUPTION: used count mismatch (stored=%u, actual=%u)\n",
		    alloc->ba_used_blocks,
		    actual_used);
		spinlock_release_irqrestore(&alloc->ba_lock, flags);
		return -EFAULT;
	}

	/* Check that reserved blocks are marked as allocated */
	for (uint32_t i = 0; i < alloc->ba_first_data_block; i++) {
		if (!bitmap_test_bit(alloc->ba_bitmap, i)) {
			BLKALLOC_DEBUG(
			    "CORRUPTION: reserved block %u is marked free\n",
			    i);
			spinlock_release_irqrestore(&alloc->ba_lock, flags);
			return -EFAULT;
		}
	}

	spinlock_release_irqrestore(&alloc->ba_lock, flags);

	BLKALLOC_DEBUG("Validation passed\n");
	return 0;
}