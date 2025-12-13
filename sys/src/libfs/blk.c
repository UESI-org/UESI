/* Complex Software, keep heavely commented !!!*/

#include <blk.h>
#include <errno.h>
#include <sys/panic.h>
#include <kmalloc.h>
#include <kfree.h>
#include <string.h>

static blk_device_t *blk_devices = NULL;
static spinlock_t blk_devices_lock = SPINLOCK_INITIALIZER("blk_devices");

#define BLK_BUFFER_HASH_SIZE 256
static blk_buffer_t *blk_buffer_hash[BLK_BUFFER_HASH_SIZE];
static spinlock_t blk_buffer_lock = SPINLOCK_INITIALIZER("blk_buffer");

static inline uint32_t
blk_buffer_hash_func(dev_t dev, uint64_t block)
{
	return (dev ^ (uint32_t)block) % BLK_BUFFER_HASH_SIZE;
}

int
blk_register_device(blk_device_t *dev)
{
	if (dev == NULL || dev->bd_ops == NULL) {
		return -EINVAL;
	}

	spinlock_init(&dev->bd_lock, "blk_dev");

	uint64_t flags;
	spinlock_acquire_irqsave(&blk_devices_lock, &flags);

	/* Add to device list */
	dev->bd_next = blk_devices;
	blk_devices = dev;

	spinlock_release_irqrestore(&blk_devices_lock, flags);

	return 0;
}

int
blk_unregister_device(dev_t dev)
{
	uint64_t flags;
	spinlock_acquire_irqsave(&blk_devices_lock, &flags);

	blk_device_t **current = &blk_devices;
	while (*current != NULL) {
		if ((*current)->bd_dev == dev) {
			blk_device_t *to_remove = *current;
			*current = to_remove->bd_next;

			spinlock_release_irqrestore(&blk_devices_lock, flags);
			return 0;
		}
		current = &(*current)->bd_next;
	}

	spinlock_release_irqrestore(&blk_devices_lock, flags);
	return -ENOENT;
}

blk_device_t *
blk_get_device(dev_t dev)
{
	uint64_t flags;
	spinlock_acquire_irqsave(&blk_devices_lock, &flags);

	blk_device_t *current = blk_devices;
	while (current != NULL) {
		if (current->bd_dev == dev) {
			spinlock_release_irqrestore(&blk_devices_lock, flags);
			return current;
		}
		current = current->bd_next;
	}

	spinlock_release_irqrestore(&blk_devices_lock, flags);
	return NULL;
}

blk_device_t *
blk_find_device(const char *name)
{
	if (name == NULL) {
		return NULL;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&blk_devices_lock, &flags);

	blk_device_t *current = blk_devices;
	while (current != NULL) {
		if (strcmp(current->bd_name, name) == 0) {
			spinlock_release_irqrestore(&blk_devices_lock, flags);
			return current;
		}
		current = current->bd_next;
	}

	spinlock_release_irqrestore(&blk_devices_lock, flags);
	return NULL;
}

int
blk_read(blk_device_t *dev, uint64_t block, void *buffer, size_t count)
{
	if (dev == NULL || buffer == NULL || count == 0) {
		return -EINVAL;
	}

	if (block + count > dev->bd_total_blocks) {
		return -EINVAL;
	}

	if (dev->bd_ops == NULL || dev->bd_ops->read == NULL) {
		return -ENOSYS;
	}

	return dev->bd_ops->read(dev, block, buffer, count);
}

int
blk_write(blk_device_t *dev, uint64_t block, const void *buffer, size_t count)
{
	if (dev == NULL || buffer == NULL || count == 0) {
		return -EINVAL;
	}

	if (block + count > dev->bd_total_blocks) {
		return -EINVAL;
	}

	if (dev->bd_flags & BLK_DEV_READONLY) {
		return -EROFS;
	}

	if (dev->bd_ops == NULL || dev->bd_ops->write == NULL) {
		return -ENOSYS;
	}

	return dev->bd_ops->write(dev, block, buffer, count);
}

int
blk_flush(blk_device_t *dev)
{
	if (dev == NULL) {
		return -EINVAL;
	}

	if (dev->bd_ops && dev->bd_ops->flush) {
		return dev->bd_ops->flush(dev);
	}

	return 0;
}

int
blk_read_block(blk_device_t *dev, uint64_t block, void *buffer)
{
	return blk_read(dev, block, buffer, 1);
}

int
blk_write_block(blk_device_t *dev, uint64_t block, const void *buffer)
{
	return blk_write(dev, block, buffer, 1);
}

int
blk_alloc_init(blk_allocator_t *alloc,
               uint32_t total_blocks,
               uint32_t first_data_block,
               uint32_t block_size)
{
	if (!alloc || total_blocks == 0) {
		return -EINVAL;
	}

	/* Calculate bitmap size (1 bit per block) */
	uint32_t bitmap_bytes = (total_blocks + 7) / 8;

	alloc->ba_bitmap = kmalloc(bitmap_bytes);
	if (!alloc->ba_bitmap) {
		return -ENOMEM;
	}

	/* Initialize all blocks as free */
	memset(alloc->ba_bitmap, 0, bitmap_bytes);

	/* Initialize allocator fields */
	alloc->ba_total_blocks = total_blocks;
	alloc->ba_used_blocks = 0;
	alloc->ba_first_data_block = first_data_block;
	alloc->ba_first_free = first_data_block;
	alloc->ba_block_size = block_size;
	alloc->ba_bitmap_size = bitmap_bytes;
	alloc->ba_dirty = false;
	spinlock_init(&alloc->ba_lock, "blk_alloc");

	return 0;
}

void
blk_alloc_destroy(blk_allocator_t *alloc)
{
	if (alloc == NULL) {
		return;
	}

	if (alloc->ba_bitmap != NULL) {
		kfree(alloc->ba_bitmap);
		alloc->ba_bitmap = NULL;
	}
}

uint32_t
blk_alloc(blk_allocator_t *alloc)
{
	if (alloc == NULL) {
		return 0;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&alloc->ba_lock, &flags);

	/* Check if any blocks available */
	if (alloc->ba_used_blocks >= alloc->ba_total_blocks) {
		spinlock_release_irqrestore(&alloc->ba_lock, flags);
		return 0;
	}

	/* Search for free block starting from hint */
	uint32_t start = alloc->ba_first_free;
	uint32_t block;

	for (block = start; block < alloc->ba_total_blocks; block++) {
		uint32_t byte = block / 8;
		uint32_t bit = block % 8;

		if ((alloc->ba_bitmap[byte] & (1 << bit)) == 0) {
			/* Found free block */
			alloc->ba_bitmap[byte] |= (1 << bit);
			alloc->ba_used_blocks++;
			alloc->ba_first_free = block + 1;

			spinlock_release_irqrestore(&alloc->ba_lock, flags);
			return block;
		}
	}

	/* Wrap around and search from beginning */
	for (block = 0; block < start; block++) {
		uint32_t byte = block / 8;
		uint32_t bit = block % 8;

		if ((alloc->ba_bitmap[byte] & (1 << bit)) == 0) {
			alloc->ba_bitmap[byte] |= (1 << bit);
			alloc->ba_used_blocks++;
			alloc->ba_first_free = block + 1;

			spinlock_release_irqrestore(&alloc->ba_lock, flags);
			return block;
		}
	}

	spinlock_release_irqrestore(&alloc->ba_lock, flags);
	return 0; /* No free blocks */
}

uint32_t
blk_alloc_contiguous(blk_allocator_t *alloc, uint32_t count, uint32_t *blocks)
{
	if (alloc == NULL || count == 0) {
		return 0;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&alloc->ba_lock, &flags);

	if (alloc->ba_used_blocks + count > alloc->ba_total_blocks) {
		spinlock_release_irqrestore(&alloc->ba_lock, flags);
		return 0;
	}

	/* Search for contiguous free blocks */
	for (uint32_t start = 0; start <= alloc->ba_total_blocks - count;
	     start++) {
		bool found = true;

		/* Check if 'count' blocks starting from 'start' are all free */
		for (uint32_t i = 0; i < count; i++) {
			uint32_t block = start + i;
			uint32_t byte = block / 8;
			uint32_t bit = block % 8;

			if (alloc->ba_bitmap[byte] & (1 << bit)) {
				found = false;
				break;
			}
		}

		if (found) {
			/* Allocate all blocks */
			for (uint32_t i = 0; i < count; i++) {
				uint32_t block = start + i;
				uint32_t byte = block / 8;
				uint32_t bit = block % 8;
				alloc->ba_bitmap[byte] |= (1 << bit);
			}

			alloc->ba_used_blocks += count;
			alloc->ba_first_free = start + count;

			spinlock_release_irqrestore(&alloc->ba_lock, flags);
			return start;
		}
	}

	spinlock_release_irqrestore(&alloc->ba_lock, flags);
	return 0;
}

int
blk_free(blk_allocator_t *alloc, uint32_t block)
{
	if (alloc == NULL || block >= alloc->ba_total_blocks) {
		return -EINVAL;
	}

	uint32_t byte = block / 8;
	uint32_t bit = block % 8;

	uint64_t flags;
	spinlock_acquire_irqsave(&alloc->ba_lock, &flags);

	/* Check if already free */
	if ((alloc->ba_bitmap[byte] & (1 << bit)) == 0) {
		spinlock_release_irqrestore(&alloc->ba_lock, flags);
		return -EINVAL;
	}

	/* Mark as free */
	alloc->ba_bitmap[byte] &= ~(1 << bit);
	alloc->ba_used_blocks--;

	if (block < alloc->ba_first_free) {
		alloc->ba_first_free = block;
	}

	spinlock_release_irqrestore(&alloc->ba_lock, flags);
	return 0;
}

int
blk_free_contiguous(blk_allocator_t *alloc, uint32_t block, uint32_t count)
{
	if (alloc == NULL || block + count > alloc->ba_total_blocks) {
		return -EINVAL;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&alloc->ba_lock, &flags);

	/* Free all blocks */
	for (uint32_t i = 0; i < count; i++) {
		uint32_t b = block + i;
		uint32_t byte = b / 8;
		uint32_t bit = b % 8;

		if (alloc->ba_bitmap[byte] & (1 << bit)) {
			alloc->ba_bitmap[byte] &= ~(1 << bit);
			alloc->ba_used_blocks--;
		}
	}

	if (block < alloc->ba_first_free) {
		alloc->ba_first_free = block;
	}

	spinlock_release_irqrestore(&alloc->ba_lock, flags);
	return 0;
}

bool
blk_is_allocated(blk_allocator_t *alloc, uint32_t block)
{
	if (alloc == NULL || block >= alloc->ba_total_blocks) {
		return false;
	}

	uint32_t byte = block / 8;
	uint32_t bit = block % 8;

	uint64_t flags;
	spinlock_acquire_irqsave(&alloc->ba_lock, &flags);
	bool allocated = (alloc->ba_bitmap[byte] & (1 << bit)) != 0;
	spinlock_release_irqrestore(&alloc->ba_lock, flags);

	return allocated;
}

void
blk_alloc_stats(blk_allocator_t *alloc,
                uint32_t *total,
                uint32_t *used,
                uint32_t *free)
{
	if (alloc == NULL) {
		return;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&alloc->ba_lock, &flags);

	if (total)
		*total = alloc->ba_total_blocks;
	if (used)
		*used = alloc->ba_used_blocks;
	if (free)
		*free = alloc->ba_total_blocks - alloc->ba_used_blocks;

	spinlock_release_irqrestore(&alloc->ba_lock, flags);
}

int
blk_mark_used(blk_allocator_t *alloc, uint32_t block)
{
	if (alloc == NULL || block >= alloc->ba_total_blocks) {
		return -EINVAL;
	}

	uint32_t byte = block / 8;
	uint32_t bit = block % 8;

	uint64_t flags;
	spinlock_acquire_irqsave(&alloc->ba_lock, &flags);

	if ((alloc->ba_bitmap[byte] & (1 << bit)) == 0) {
		alloc->ba_bitmap[byte] |= (1 << bit);
		alloc->ba_used_blocks++;
	}

	spinlock_release_irqrestore(&alloc->ba_lock, flags);
	return 0;
}

int
blk_mark_range_used(blk_allocator_t *alloc, uint32_t start, uint32_t count)
{
	if (alloc == NULL || start + count > alloc->ba_total_blocks) {
		return -EINVAL;
	}

	for (uint32_t i = 0; i < count; i++) {
		blk_mark_used(alloc, start + i);
	}

	return 0;
}

int
blk_buffer_init(void)
{
	memset(blk_buffer_hash, 0, sizeof(blk_buffer_hash));
	return 0;
}

int
blk_buffer_get(blk_device_t *dev, uint64_t block, blk_buffer_t **buf)
{
	if (dev == NULL || buf == NULL) {
		return -EINVAL;
	}

	uint32_t hash = blk_buffer_hash_func(dev->bd_dev, block);

	uint64_t flags;
	spinlock_acquire_irqsave(&blk_buffer_lock, &flags);

	/* Search in cache */
	blk_buffer_t *buffer = blk_buffer_hash[hash];
	while (buffer != NULL) {
		if (buffer->bb_dev == dev &&
		    buffer->bb_block == (int64_t)block) {
			/* Found in cache */
			spinlock_acquire(&buffer->bb_lock);
			buffer->bb_refcount++;
			spinlock_release(&buffer->bb_lock);

			spinlock_release_irqrestore(&blk_buffer_lock, flags);
			*buf = buffer;
			return 0;
		}
		buffer = buffer->bb_next;
	}

	spinlock_release_irqrestore(&blk_buffer_lock, flags);

	/* Not in cache - allocate new buffer */
	buffer = kmalloc(sizeof(blk_buffer_t));
	if (buffer == NULL) {
		return -ENOMEM;
	}

	buffer->bb_data = kmalloc(dev->bd_block_size);
	if (buffer->bb_data == NULL) {
		kfree(buffer);
		return -ENOMEM;
	}

	buffer->bb_block = block;
	buffer->bb_dev = dev;
	buffer->bb_size = dev->bd_block_size;
	buffer->bb_flags = 0;
	buffer->bb_refcount = 1;
	spinlock_init(&buffer->bb_lock, "blk_buffer");

	/* Read block from disk */
	int ret = blk_read_block(dev, block, buffer->bb_data);
	if (ret != 0) {
		kfree(buffer->bb_data);
		kfree(buffer);
		return ret;
	}

	buffer->bb_flags |= BLK_BUF_VALID;

	/* Add to cache */
	spinlock_acquire_irqsave(&blk_buffer_lock, &flags);
	buffer->bb_next = blk_buffer_hash[hash];
	blk_buffer_hash[hash] = buffer;
	spinlock_release_irqrestore(&blk_buffer_lock, flags);

	*buf = buffer;
	return 0;
}

void
blk_buffer_put(blk_buffer_t *buf)
{
	if (buf == NULL) {
		return;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&buf->bb_lock, &flags);

	if (buf->bb_refcount > 0) {
		buf->bb_refcount--;

		if (buf->bb_refcount == 0) {
			/* Write dirty buffer */
			if (buf->bb_flags & BLK_BUF_DIRTY) {
				spinlock_release_irqrestore(&buf->bb_lock,
				                            flags);
				blk_buffer_sync(buf);
				return;
			}
		}
	}

	spinlock_release_irqrestore(&buf->bb_lock, flags);
}

void
blk_buffer_mark_dirty(blk_buffer_t *buf)
{
	if (buf == NULL) {
		return;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&buf->bb_lock, &flags);
	buf->bb_flags |= BLK_BUF_DIRTY;
	spinlock_release_irqrestore(&buf->bb_lock, flags);
}

int
blk_buffer_sync(blk_buffer_t *buf)
{
	if (buf == NULL) {
		return -EINVAL;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&buf->bb_lock, &flags);

	if (!(buf->bb_flags & BLK_BUF_DIRTY)) {
		spinlock_release_irqrestore(&buf->bb_lock, flags);
		return 0;
	}

	spinlock_release_irqrestore(&buf->bb_lock, flags);

	/* Write to disk */
	int ret = blk_write_block(buf->bb_dev, buf->bb_block, buf->bb_data);

	if (ret == 0) {
		spinlock_acquire_irqsave(&buf->bb_lock, &flags);
		buf->bb_flags &= ~BLK_BUF_DIRTY;
		spinlock_release_irqrestore(&buf->bb_lock, flags);
	}

	return ret;
}

int
blk_buffer_sync_all(void)
{
	uint64_t flags;
	spinlock_acquire_irqsave(&blk_buffer_lock, &flags);

	for (int i = 0; i < BLK_BUFFER_HASH_SIZE; i++) {
		blk_buffer_t *buf = blk_buffer_hash[i];
		while (buf != NULL) {
			if (buf->bb_flags & BLK_BUF_DIRTY) {
				spinlock_release_irqrestore(&blk_buffer_lock,
				                            flags);
				blk_buffer_sync(buf);
				spinlock_acquire_irqsave(&blk_buffer_lock,
				                         &flags);
			}
			buf = buf->bb_next;
		}
	}

	spinlock_release_irqrestore(&blk_buffer_lock, flags);
	return 0;
}

typedef struct ramdisk_data {
	void *data;
	size_t size;
} ramdisk_data_t;

static int
ramdisk_read(blk_device_t *dev, uint64_t block, void *buffer, size_t count)
{
	ramdisk_data_t *rd = (ramdisk_data_t *)dev->bd_private;

	uint64_t offset = block * dev->bd_block_size;
	size_t bytes = count * dev->bd_block_size;

	if (offset + bytes > rd->size) {
		return -EINVAL;
	}

	memcpy(buffer, (uint8_t *)rd->data + offset, bytes);
	return 0;
}

static int
ramdisk_write(blk_device_t *dev,
              uint64_t block,
              const void *buffer,
              size_t count)
{
	ramdisk_data_t *rd = (ramdisk_data_t *)dev->bd_private;

	uint64_t offset = block * dev->bd_block_size;
	size_t bytes = count * dev->bd_block_size;

	if (offset + bytes > rd->size) {
		return -EINVAL;
	}

	memcpy((uint8_t *)rd->data + offset, buffer, bytes);
	return 0;
}

static blk_ops_t ramdisk_ops = { .read = ramdisk_read,
	                         .write = ramdisk_write,
	                         .flush = NULL,
	                         .get_info = NULL };

blk_device_t *
blk_ramdisk_create(const char *name, size_t size_bytes)
{
	blk_device_t *dev = kmalloc(sizeof(blk_device_t));
	if (dev == NULL) {
		return NULL;
	}

	ramdisk_data_t *rd = kmalloc(sizeof(ramdisk_data_t));
	if (rd == NULL) {
		kfree(dev);
		return NULL;
	}

	rd->data = kmalloc(size_bytes);
	if (rd->data == NULL) {
		kfree(rd);
		kfree(dev);
		return NULL;
	}

	memset(rd->data, 0, size_bytes);
	rd->size = size_bytes;

	dev->bd_name = name;
	dev->bd_flags = BLK_DEV_VIRTUAL;
	dev->bd_block_size = BLK_SIZE_DEFAULT;
	dev->bd_total_blocks = size_bytes / BLK_SIZE_DEFAULT;
	dev->bd_ops = &ramdisk_ops;
	dev->bd_private = rd;

	blk_register_device(dev);

	return dev;
}

int
blk_ramdisk_destroy(blk_device_t *dev)
{
	if (dev == NULL) {
		return -EINVAL;
	}

	ramdisk_data_t *rd = (ramdisk_data_t *)dev->bd_private;
	if (rd != NULL) {
		if (rd->data != NULL) {
			kfree(rd->data);
		}
		kfree(rd);
	}

	blk_unregister_device(dev->bd_dev);
	kfree(dev);

	return 0;
}

void
blk_dump_devices(void)
{
	uint64_t flags;
	spinlock_acquire_irqsave(&blk_devices_lock, &flags);

	blk_device_t *dev = blk_devices;
	int count = 0;

	while (dev != NULL) {
		count++;
		dev = dev->bd_next;
	}

	spinlock_release_irqrestore(&blk_devices_lock, flags);
}

void
blk_dump_allocator(blk_allocator_t *alloc)
{
	if (alloc == NULL) {
		return;
	}

	uint32_t total, used, free;
	blk_alloc_stats(alloc, &total, &used, &free);
}