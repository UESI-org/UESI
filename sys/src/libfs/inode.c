/* Complex Software, keep heavely commented !!!*/

#include <inode.h>
#include <errno.h>
#include <sys/panic.h>
#include <kmalloc.h>
#include <blkalloc.h>
#include <kfree.h>
#include <string.h>

/* Inode cache hash table */
#define INODE_HASH_SIZE 256
static inode_t *inode_hash[INODE_HASH_SIZE];
static spinlock_t inode_hash_lock = SPINLOCK_INITIALIZER("inode_hash");

static superblock_t *dirty_superblocks[32]; /* Max 32 mounted filesystems */
static int dirty_sb_count = 0;
static spinlock_t dirty_sb_lock = SPINLOCK_INITIALIZER("dirty_sb");

/* Hash function for inode cache */
static inline uint32_t
inode_hash_func(ino_t ino)
{
	return ino % INODE_HASH_SIZE;
}

/*
 * Inode Allocator Functions
 */

int
inode_alloc_init(inode_alloc_t *alloc, uint32_t total_inodes)
{
	if (alloc == NULL || total_inodes == 0) {
		return -EINVAL;
	}

	/* Calculate bitmap size (1 bit per inode) */
	uint32_t bitmap_bytes = (total_inodes + 7) / 8;

	alloc->ia_bitmap = kmalloc(bitmap_bytes);
	if (alloc->ia_bitmap == NULL) {
		return -ENOMEM;
	}

	/* Initialize all inodes as free */
	memset(alloc->ia_bitmap, 0, bitmap_bytes);

	alloc->ia_total = total_inodes;
	alloc->ia_used = 0;
	alloc->ia_first_free = 0;
	spinlock_init(&alloc->ia_lock, "inode_alloc");

	return 0;
}

void
inode_alloc_destroy(inode_alloc_t *alloc)
{
	if (alloc == NULL) {
		return;
	}

	if (alloc->ia_bitmap != NULL) {
		kfree(alloc->ia_bitmap);
		alloc->ia_bitmap = NULL;
	}
}

ino_t
inode_alloc(inode_alloc_t *alloc)
{
	if (alloc == NULL) {
		return 0;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&alloc->ia_lock, &flags);

	/* Check if any inodes available */
	if (alloc->ia_used >= alloc->ia_total) {
		spinlock_release_irqrestore(&alloc->ia_lock, flags);
		return 0;
	}

	/* Search for free inode starting from hint */
	uint32_t start = alloc->ia_first_free;
	uint32_t ino;

	for (ino = start; ino < alloc->ia_total; ino++) {
		uint32_t byte = ino / 8;
		uint32_t bit = ino % 8;

		if ((alloc->ia_bitmap[byte] & (1 << bit)) == 0) {
			/* Found free inode */
			alloc->ia_bitmap[byte] |= (1 << bit);
			alloc->ia_used++;
			alloc->ia_first_free = ino + 1;

			spinlock_release_irqrestore(&alloc->ia_lock, flags);
			return ino + 1; /* Inode numbers start at 1 */
		}
	}

	/* Wrap around and search from beginning */
	for (ino = 0; ino < start; ino++) {
		uint32_t byte = ino / 8;
		uint32_t bit = ino % 8;

		if ((alloc->ia_bitmap[byte] & (1 << bit)) == 0) {
			alloc->ia_bitmap[byte] |= (1 << bit);
			alloc->ia_used++;
			alloc->ia_first_free = ino + 1;

			spinlock_release_irqrestore(&alloc->ia_lock, flags);
			return ino + 1;
		}
	}

	spinlock_release_irqrestore(&alloc->ia_lock, flags);
	return 0; /* No free inodes */
}

int
inode_free(inode_alloc_t *alloc, ino_t ino)
{
	if (alloc == NULL || ino == 0 || ino > alloc->ia_total) {
		return -EINVAL;
	}

	uint32_t index = ino - 1; /* Convert to 0-based */
	uint32_t byte = index / 8;
	uint32_t bit = index % 8;

	uint64_t flags;
	spinlock_acquire_irqsave(&alloc->ia_lock, &flags);

	/* Check if already free */
	if ((alloc->ia_bitmap[byte] & (1 << bit)) == 0) {
		spinlock_release_irqrestore(&alloc->ia_lock, flags);
		return -EINVAL;
	}

	/* Mark as free */
	alloc->ia_bitmap[byte] &= ~(1 << bit);
	alloc->ia_used--;

	if (index < alloc->ia_first_free) {
		alloc->ia_first_free = index;
	}

	spinlock_release_irqrestore(&alloc->ia_lock, flags);
	return 0;
}

bool
inode_is_allocated(inode_alloc_t *alloc, ino_t ino)
{
	if (alloc == NULL || ino == 0 || ino > alloc->ia_total) {
		return false;
	}

	uint32_t index = ino - 1;
	uint32_t byte = index / 8;
	uint32_t bit = index % 8;

	uint64_t flags;
	spinlock_acquire_irqsave(&alloc->ia_lock, &flags);
	bool allocated = (alloc->ia_bitmap[byte] & (1 << bit)) != 0;
	spinlock_release_irqrestore(&alloc->ia_lock, flags);

	return allocated;
}

void
inode_alloc_stats(inode_alloc_t *alloc,
                  uint32_t *total,
                  uint32_t *used,
                  uint32_t *free)
{
	if (alloc == NULL) {
		return;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&alloc->ia_lock, &flags);

	if (total)
		*total = alloc->ia_total;
	if (used)
		*used = alloc->ia_used;
	if (free)
		*free = alloc->ia_total - alloc->ia_used;

	spinlock_release_irqrestore(&alloc->ia_lock, flags);
}

/*
 * Inode Cache Functions
 */

int
inode_cache_init(void)
{
	memset(inode_hash, 0, sizeof(inode_hash));
	return 0;
}

inode_t *
inode_alloc_mem(struct vfs_mount *mnt, ino_t ino)
{
	inode_t *inode = kmalloc(sizeof(inode_t));
	if (inode == NULL) {
		return NULL;
	}

	memset(inode, 0, sizeof(inode_t));

	inode->i_ino = ino;
	inode->i_refcount = 1;
	inode->i_flags = INODE_FLAG_VALID;
	inode->i_mount = mnt;
	spinlock_init(&inode->i_lock, "inode");

	/* Add to hash table */
	uint32_t hash = inode_hash_func(ino);

	uint64_t flags;
	spinlock_acquire_irqsave(&inode_hash_lock, &flags);
	inode->i_next = inode_hash[hash];
	inode_hash[hash] = inode;
	spinlock_release_irqrestore(&inode_hash_lock, flags);

	return inode;
}

void
inode_free_mem(inode_t *inode)
{
	if (inode == NULL) {
		return;
	}

	/* Remove from hash table */
	uint32_t hash = inode_hash_func(inode->i_ino);

	uint64_t flags;
	spinlock_acquire_irqsave(&inode_hash_lock, &flags);

	inode_t **current = &inode_hash[hash];
	while (*current != NULL) {
		if (*current == inode) {
			*current = inode->i_next;
			break;
		}
		current = &(*current)->i_next;
	}

	spinlock_release_irqrestore(&inode_hash_lock, flags);

	kfree(inode);
}

int
inode_get(struct vfs_mount *mnt, ino_t ino, inode_t **result)
{
	if (mnt == NULL || ino == 0 || result == NULL) {
		return -EINVAL;
	}

	uint32_t hash = inode_hash_func(ino);

	uint64_t flags;
	spinlock_acquire_irqsave(&inode_hash_lock, &flags);

	/* Search in cache */
	inode_t *inode = inode_hash[hash];
	while (inode != NULL) {
		if (inode->i_ino == ino && inode->i_mount == mnt) {
			/* Found in cache */
			spinlock_acquire(&inode->i_lock);
			inode->i_refcount++;
			spinlock_release(&inode->i_lock);

			spinlock_release_irqrestore(&inode_hash_lock, flags);
			*result = inode;
			return 0;
		}
		inode = inode->i_next;
	}

	spinlock_release_irqrestore(&inode_hash_lock, flags);

	/* Not in cache - allocate new */
	inode = inode_alloc_mem(mnt, ino);
	if (inode == NULL) {
		return -ENOMEM;
	}

	/* TODO: Load inode data from disk here */
	/* This would be done by the filesystem driver */

	*result = inode;
	return 0;
}

void
inode_put(inode_t *inode)
{
	if (inode == NULL) {
		return;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&inode->i_lock, &flags);

	if (inode->i_refcount > 0) {
		inode->i_refcount--;

		if (inode->i_refcount == 0) {
			/* Write dirty inode to disk */
			if (inode->i_flags & INODE_FLAG_DIRTY) {
				spinlock_release_irqrestore(&inode->i_lock,
				                            flags);
				inode_sync(inode);
				spinlock_acquire_irqsave(&inode->i_lock,
				                         &flags);
			}

			spinlock_release_irqrestore(&inode->i_lock, flags);
			inode_free_mem(inode);
			return;
		}
	}

	spinlock_release_irqrestore(&inode->i_lock, flags);
}

void
inode_ref(inode_t *inode)
{
	if (inode == NULL) {
		return;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&inode->i_lock, &flags);
	inode->i_refcount++;
	spinlock_release_irqrestore(&inode->i_lock, flags);
}

void
inode_mark_dirty(inode_t *inode)
{
	if (inode == NULL) {
		return;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&inode->i_lock, &flags);
	inode->i_flags |= INODE_FLAG_DIRTY;
	spinlock_release_irqrestore(&inode->i_lock, flags);
}

int
inode_sync(inode_t *inode)
{
	if (inode == NULL) {
		return -EINVAL;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&inode->i_lock, &flags);

	if (!(inode->i_flags & INODE_FLAG_DIRTY)) {
		spinlock_release_irqrestore(&inode->i_lock, flags);
		return 0;
	}

	/* TODO: Write inode to disk */
	/* This would be done by the filesystem driver */

	inode->i_flags &= ~INODE_FLAG_DIRTY;

	spinlock_release_irqrestore(&inode->i_lock, flags);
	return 0;
}

int
inode_sync_all(void)
{
	uint64_t flags;
	spinlock_acquire_irqsave(&inode_hash_lock, &flags);

	for (int i = 0; i < INODE_HASH_SIZE; i++) {
		inode_t *inode = inode_hash[i];
		while (inode != NULL) {
			if (inode->i_flags & INODE_FLAG_DIRTY) {
				inode_ref(inode);
				spinlock_release_irqrestore(&inode_hash_lock,
				                            flags);

				inode_sync(inode);

				spinlock_acquire_irqsave(&inode_hash_lock,
				                         &flags);
				inode_put(inode);
			}
			inode = inode->i_next;
		}
	}

	spinlock_release_irqrestore(&inode_hash_lock, flags);
	return 0;
}

/*
 * Block mapping functions
 */

int
inode_get_block(inode_t *inode, uint64_t file_block, uint32_t *disk_block)
{
	if (inode == NULL || disk_block == NULL) {
		return -EINVAL;
	}

	/* Direct blocks */
	if (file_block < INODE_DIRECT_BLOCKS) {
		*disk_block = inode->i_disk.di_block[file_block];
		return 0;
	}

	/* TODO: Handle indirect blocks */
	/* This requires reading indirect block pointers from disk */

	return -ENOSYS;
}

int
inode_alloc_block(inode_t *inode, uint64_t file_block, uint32_t *disk_block)
{
	if (inode == NULL || disk_block == NULL) {
		return -EINVAL;
	}

	// Get block allocator from mount point
	blk_allocator_t *alloc = inode->i_mount->mnt_private;

	// Allocate new block
	uint32_t new_block = blk_alloc(alloc);
	if (new_block == 0) {
		return -ENOSPC;
	}

	// Store in appropriate location (direct/indirect)
	if (file_block < INODE_DIRECT_BLOCKS) {
		inode->i_disk.di_block[file_block] = new_block;
	} else {
	}

	inode->i_disk.di_blocks++;
	inode_mark_dirty(inode);
	*disk_block = new_block;

	return 0;
}

int
inode_truncate_blocks(inode_t *inode)
{
	blk_allocator_t *alloc = inode->i_mount->mnt_private;

	// Free all direct blocks
	for (int i = 0; i < INODE_DIRECT_BLOCKS; i++) {
		if (inode->i_disk.di_block[i] != 0) {
			blk_free(alloc, inode->i_disk.di_block[i]);
			inode->i_disk.di_block[i] = 0;
		}
	}

	inode->i_disk.di_size = 0;
	inode->i_disk.di_blocks = 0;
	inode_mark_dirty(inode);

	return 0;
}

int
inode_to_vnode(inode_t *inode, vnode_t **vnode)
{
	if (inode == NULL || vnode == NULL) {
		return -EINVAL;
	}

	if (inode->i_vnode != NULL) {
		*vnode = inode->i_vnode;
		vfs_vnode_ref(*vnode);
		return 0;
	}

	/* Create new vnode */
	vnode_t *v = vfs_vnode_alloc(inode->i_mount, inode->i_ino);
	if (v == NULL) {
		return -ENOMEM;
	}

	/* Copy metadata */
	v->v_mode = inode->i_disk.di_mode;
	v->v_uid = inode->i_disk.di_uid;
	v->v_gid = inode->i_disk.di_gid;
	v->v_size = inode->i_disk.di_size;
	v->v_nlink = inode->i_disk.di_nlink;
	v->v_atime = inode->i_disk.di_atime;
	v->v_mtime = inode->i_disk.di_mtime;
	v->v_ctime = inode->i_disk.di_ctime;
	v->v_private = inode;

	inode->i_vnode = v;
	*vnode = v;

	return 0;
}

inode_t *
vnode_to_inode(vnode_t *vnode)
{
	if (vnode == NULL) {
		return NULL;
	}
	return (inode_t *)vnode->v_private;
}

int
superblock_init(superblock_t *sb,
                uint32_t block_size,
                uint32_t total_blocks,
                uint32_t total_inodes)
{
	if (sb == NULL) {
		return -EINVAL;
	}

	memset(sb, 0, sizeof(superblock_t));

	sb->sb_block_size = block_size;
	sb->sb_total_blocks = total_blocks;
	sb->sb_free_blocks = total_blocks;
	sb->sb_total_inodes = total_inodes;
	sb->sb_free_inodes = total_inodes;
	sb->sb_state = SB_STATE_CLEAN;
	sb->sb_errors = SB_ERRORS_CONTINUE;

	return 0;
}

int
superblock_read(struct vfs_mount *mnt, uint32_t block, superblock_t *sb)
{
	if (mnt == NULL || sb == NULL) {
		return -EINVAL;
	}

	/* Get the device file for this mount */
	if (mnt->mnt_device == NULL) {
		return -EINVAL;
	}

	/* Open the block device */
	vfs_file_t *dev_file;
	int ret = vfs_open(mnt->mnt_device, VFS_O_RDONLY, 0, &dev_file);
	if (ret != 0) {
		return ret;
	}

	/* Calculate byte offset: block number * 512 (standard block size) */
	/* We use 512 here because we don't know the actual block size yet */
	off_t offset = (off_t)block * 512;

	/* Seek to the superblock location */
	off_t seek_result = vfs_seek(dev_file, offset, VFS_SEEK_SET);
	if (seek_result < 0) {
		vfs_close(dev_file);
		return seek_result;
	}

	/* Read the superblock structure from disk */
	ssize_t bytes_read = vfs_read(dev_file, sb, sizeof(superblock_t));

	/* Close the device file */
	vfs_close(dev_file);

	if (bytes_read != sizeof(superblock_t)) {
		return bytes_read < 0 ? bytes_read : -EIO;
	}

	return 0;
}

int
superblock_write(struct vfs_mount *mnt, uint32_t block, superblock_t *sb)
{
	if (mnt == NULL || sb == NULL) {
		return -EINVAL;
	}

	/* Get the device file for this mount */
	if (mnt->mnt_device == NULL) {
		return -EINVAL;
	}

	/* Open the block device */
	vfs_file_t *dev_file;
	int ret = vfs_open(mnt->mnt_device, VFS_O_RDWR, 0, &dev_file);
	if (ret != 0) {
		return ret;
	}

	/* Calculate byte offset: block number * block size */
	off_t offset = (off_t)block * sb->sb_block_size;

	/* Seek to the superblock location */
	off_t seek_result = vfs_seek(dev_file, offset, VFS_SEEK_SET);
	if (seek_result < 0) {
		vfs_close(dev_file);
		return seek_result;
	}

	/* Write the superblock structure to disk */
	ssize_t bytes_written = vfs_write(dev_file, sb, sizeof(superblock_t));

	if (bytes_written != sizeof(superblock_t)) {
		vfs_close(dev_file);
		return bytes_written < 0 ? bytes_written : -EIO;
	}

	/* Sync to ensure data is written to disk */
	ret = vfs_fsync(dev_file);

	/* Close the device file */
	vfs_close(dev_file);

	return ret;
}

bool
superblock_validate(superblock_t *sb, uint32_t expected_magic)
{
	if (sb == NULL) {
		return false;
	}

	return sb->sb_magic == expected_magic && sb->sb_block_size > 0 &&
	       sb->sb_total_blocks > 0 && sb->sb_total_inodes > 0;
}

int
superblock_sync(struct vfs_mount *mnt, uint32_t block, superblock_t *sb)
{
	if (sb == NULL || mnt == NULL) {
		return -EINVAL;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&dirty_sb_lock, &flags);

	if (!(sb->sb_flags & SB_FLAG_DIRTY)) {
		spinlock_release_irqrestore(&dirty_sb_lock, flags);
		return 0; /* Not dirty, nothing to do */
	}

	spinlock_release_irqrestore(&dirty_sb_lock, flags);

	/* Write superblock to disk */
	int ret = superblock_write(mnt, block, sb);
	if (ret != 0) {
		return ret;
	}

	/* Clear dirty flag after successful write */
	spinlock_acquire_irqsave(&dirty_sb_lock, &flags);
	sb->sb_flags &= ~SB_FLAG_DIRTY;

	/* Remove from dirty list */
	for (int i = 0; i < dirty_sb_count; i++) {
		if (dirty_superblocks[i] == sb) {
			/* Shift remaining entries */
			for (int j = i; j < dirty_sb_count - 1; j++) {
				dirty_superblocks[j] = dirty_superblocks[j + 1];
			}
			dirty_sb_count--;
			break;
		}
	}

	spinlock_release_irqrestore(&dirty_sb_lock, flags);
	return 0;
}

int
superblock_sync_all(void)
{
	uint64_t flags;
	spinlock_acquire_irqsave(&dirty_sb_lock, &flags);

	/* For a complete implementation, track (sb, mnt, block) tuples */
	for (int i = 0; i < dirty_sb_count; i++) {
		superblock_t *sb = dirty_superblocks[i];
		if (sb && (sb->sb_flags & SB_FLAG_DIRTY)) {
			/* TODO: Call superblock_sync() with proper mount and
			 * block */
			/* This requires storing mount point info with each
			 * dirty sb */
			sb->sb_flags &= ~SB_FLAG_DIRTY;
		}
	}

	/* Clear the dirty list */
	dirty_sb_count = 0;

	spinlock_release_irqrestore(&dirty_sb_lock, flags);
	return 0;
}

void
superblock_mark_dirty(superblock_t *sb)
{
	if (sb == NULL) {
		return;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&dirty_sb_lock, &flags);

	/* Set dirty flag */
	sb->sb_flags |= SB_FLAG_DIRTY;

	/* Check if already in dirty list */
	bool already_tracked = false;
	for (int i = 0; i < dirty_sb_count; i++) {
		if (dirty_superblocks[i] == sb) {
			already_tracked = true;
			break;
		}
	}

	/* Add to dirty list if not already there */
	if (!already_tracked && dirty_sb_count < 32) {
		dirty_superblocks[dirty_sb_count++] = sb;
	}

	spinlock_release_irqrestore(&dirty_sb_lock, flags);
}