#ifndef _FS_INODE_H_
#define _FS_INODE_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/spinlock.h>
#include <vfs.h>
#include <stdint.h>
#include <stdbool.h>

__BEGIN_DECLS

#define SB_FLAG_DIRTY  0x01  /* Superblock needs to be written to disk */

#define INODE_FLAG_DIRTY        0x0001  /* Inode needs to be written to disk */
#define INODE_FLAG_LOCKED       0x0002  /* Inode is locked */
#define INODE_FLAG_VALID        0x0004  /* Inode contains valid data */
#define INODE_FLAG_FREE         0x0008  /* Inode is free for allocation */

#define INODE_DIRECT_BLOCKS     12
#define INODE_INDIRECT_BLOCK    12      /* Single indirect */
#define INODE_DINDIRECT_BLOCK   13      /* Double indirect */
#define INODE_TINDIRECT_BLOCK   14      /* Triple indirect */

typedef struct disk_inode {
    uint16_t    di_mode;                /* File type and permissions */
    uint16_t    di_nlink;               /* Number of hard links */
    uint32_t    di_uid;                 /* Owner user ID */
    uint32_t    di_gid;                 /* Owner group ID */
    uint64_t    di_size;                /* File size in bytes */
    uint64_t    di_atime;               /* Last access time */
    uint64_t    di_mtime;               /* Last modification time */
    uint64_t    di_ctime;               /* Last status change time */
    uint32_t    di_blocks;              /* Number of 512-byte blocks allocated */
    uint32_t    di_flags;               /* Inode flags */
    
    uint32_t    di_block[INODE_DIRECT_BLOCKS];  /* Direct blocks */
    uint32_t    di_indirect;                     /* Single indirect block */
    uint32_t    di_dindirect;                    /* Double indirect block */
    uint32_t    di_tindirect;                    /* Triple indirect block */
    
    uint32_t    di_generation;          /* File generation number */
    uint32_t    di_reserved[3];         /* Reserved for future use */
} disk_inode_t;

typedef struct inode {
    ino_t           i_ino;              /* Inode number */
    uint32_t        i_refcount;         /* Reference count */
    uint32_t        i_flags;            /* In-memory flags */
    
    disk_inode_t    i_disk;             /* On-disk inode data */
    
    vnode_t         *i_vnode;           /* Associated vnode */
    struct vfs_mount *i_mount;          /* Mounted filesystem */
    
    spinlock_t      i_lock;             /* Inode lock */
    struct inode    *i_next;            /* Next in hash chain */
} inode_t;

typedef struct inode_allocator {
    uint8_t     *ia_bitmap;             /* Inode allocation bitmap */
    uint32_t    ia_total;               /* Total number of inodes */
    uint32_t    ia_used;                /* Number of used inodes */
    uint32_t    ia_first_free;          /* Hint: first potentially free inode */
    spinlock_t  ia_lock;                /* Allocator lock */
} inode_alloc_t;

typedef struct superblock {
    uint32_t    sb_magic;               /* Magic number for identification */
    uint32_t    sb_block_size;          /* Block size in bytes */
    uint32_t    sb_total_blocks;        /* Total blocks in filesystem */
    uint32_t    sb_free_blocks;         /* Free blocks */
    uint32_t    sb_total_inodes;        /* Total inodes */
    uint32_t    sb_free_inodes;         /* Free inodes */
    uint32_t    sb_first_data_block;    /* First data block */
    uint32_t    sb_inode_table_block;   /* Block number of inode table */
    uint32_t    sb_block_bitmap_block;  /* Block bitmap location */
    uint32_t    sb_inode_bitmap_block;  /* Inode bitmap location */
    uint32_t    sb_mount_time;          /* Last mount time */
    uint32_t    sb_write_time;          /* Last write time */
    uint16_t    sb_mount_count;         /* Mount count */
    uint16_t    sb_max_mount_count;     /* Max mounts before check */
    uint16_t    sb_state;               /* Filesystem state */
    uint16_t    sb_errors;              /* Behavior on errors */
    uint16_t    sb_flags;               /* NEW: Superblock flags */
    uint16_t    sb_padding;             /* Alignment padding */
    uint32_t    sb_reserved[10];        /* Reserved for future use (reduced from 12) */
} superblock_t;

#define SB_STATE_CLEAN          0x0001  /* Filesystem is clean */
#define SB_STATE_ERROR          0x0002  /* Filesystem has errors */

#define SB_ERRORS_CONTINUE      1       /* Continue on errors */
#define SB_ERRORS_READONLY      2       /* Remount read-only */
#define SB_ERRORS_PANIC         3       /* Panic on errors */

int inode_alloc_init(inode_alloc_t *alloc, uint32_t total_inodes);

void inode_alloc_destroy(inode_alloc_t *alloc);

ino_t inode_alloc(inode_alloc_t *alloc);

int inode_free(inode_alloc_t *alloc, ino_t ino);

bool inode_is_allocated(inode_alloc_t *alloc, ino_t ino);

void inode_alloc_stats(inode_alloc_t *alloc, uint32_t *total, 
                       uint32_t *used, uint32_t *free);

int inode_cache_init(void);

inode_t *inode_alloc_mem(struct vfs_mount *mnt, ino_t ino);
void inode_free_mem(inode_t *inode);

int inode_get(struct vfs_mount *mnt, ino_t ino, inode_t **result);
void inode_put(inode_t *inode);
void inode_ref(inode_t *inode);
void inode_mark_dirty(inode_t *inode);
int inode_sync(inode_t *inode);
int inode_sync_all(void);
int inode_get_block(inode_t *inode, uint64_t file_block, uint32_t *disk_block);
int inode_alloc_block(inode_t *inode, uint64_t file_block, uint32_t *disk_block);
int inode_truncate_blocks(inode_t *inode);
int inode_to_vnode(inode_t *inode, vnode_t **vnode);

inode_t *vnode_to_inode(vnode_t *vnode);

int superblock_init(superblock_t *sb, uint32_t block_size, 
                    uint32_t total_blocks, uint32_t total_inodes);

int superblock_read(struct vfs_mount *mnt, uint32_t block, superblock_t *sb);
int superblock_write(struct vfs_mount *mnt, uint32_t block, superblock_t *sb);
bool superblock_validate(superblock_t *sb, uint32_t expected_magic);
int superblock_sync_all(void);
int superblock_sync(struct vfs_mount *mnt, uint32_t block, superblock_t *sb);
void superblock_mark_dirty(superblock_t *sb);

static inline uint32_t inode_blocks_needed(uint64_t size, uint32_t block_size) {
    return (size + block_size - 1) / block_size;
}

static inline bool inode_is_dir(inode_t *inode) {
    return (inode->i_disk.di_mode & VFS_IFMT) == VFS_IFDIR;
}

static inline bool inode_is_reg(inode_t *inode) {
    return (inode->i_disk.di_mode & VFS_IFMT) == VFS_IFREG;
}

static inline bool inode_is_lnk(inode_t *inode) {
    return (inode->i_disk.di_mode & VFS_IFMT) == VFS_IFLNK;
}

static inline uint64_t inode_get_size(inode_t *inode) {
    return inode->i_disk.di_size;
}

static inline void inode_set_size(inode_t *inode, uint64_t size) {
    inode->i_disk.di_size = size;
    inode_mark_dirty(inode);
}

__END_DECLS

#endif