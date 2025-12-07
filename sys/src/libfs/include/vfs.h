#ifndef _FS_VFS_H_
#define _FS_VFS_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/spinlock.h>
#include <stdint.h>
#include <stdbool.h>

__BEGIN_DECLS

struct vnode;
struct vfs_mount;
struct vfs_operations;
struct vnode_operations;

#define VFS_SUCCESS     0

#define VFS_IFMT        0170000         /* Type of file mask */
#define VFS_IFIFO       0010000         /* Named pipe (fifo) */
#define VFS_IFCHR       0020000         /* Character special */
#define VFS_IFDIR       0040000         /* Directory */
#define VFS_IFBLK       0060000         /* Block special */
#define VFS_IFREG       0100000         /* Regular file */
#define VFS_IFLNK       0120000         /* Symbolic link */
#define VFS_IFSOCK      0140000         /* Socket */

#define VFS_O_RDONLY    0x0001          /* Open for reading only */
#define VFS_O_WRONLY    0x0002          /* Open for writing only */
#define VFS_O_RDWR      0x0003          /* Open for reading and writing */
#define VFS_O_CREAT     0x0100          /* Create file if it doesn't exist */
#define VFS_O_EXCL      0x0200          /* Error if create and file exists */
#define VFS_O_TRUNC     0x0400          /* Truncate to zero length */
#define VFS_O_APPEND    0x0800          /* Append mode */

#define VFS_SEEK_SET    0               /* Seek from beginning */
#define VFS_SEEK_CUR    1               /* Seek from current position */
#define VFS_SEEK_END    2               /* Seek from end */

#define VFS_MAX_PATH    4096
#define VFS_MAX_NAME    256
#define VFS_MAX_SYMLINK_DEPTH 8

typedef struct vfs_stat {
    dev_t       st_dev;         /* Device ID */
    ino_t       st_ino;         /* Inode number */
    mode_t      st_mode;        /* File type and mode */
    nlink_t     st_nlink;       /* Number of hard links */
    uid_t       st_uid;         /* User ID of owner */
    gid_t       st_gid;         /* Group ID of owner */
    dev_t       st_rdev;        /* Device ID (if special file) */
    off_t       st_size;        /* Total size in bytes */
    blksize_t   st_blksize;     /* Block size for filesystem I/O */
    blkcnt_t    st_blocks;      /* Number of 512B blocks allocated */
    time_t      st_atime;       /* Time of last access */
    time_t      st_mtime;       /* Time of last modification */
    time_t      st_ctime;       /* Time of last status change */
} vfs_stat_t;

typedef struct vfs_dirent {
    ino_t       d_ino;                  /* Inode number */
    off_t       d_off;                  /* Offset to next dirent */
    uint16_t    d_reclen;               /* Length of this record */
    uint8_t     d_type;                 /* Type of file */
    char        d_name[VFS_MAX_NAME];   /* Filename */
} vfs_dirent_t;

typedef struct vnode {
    ino_t               v_ino;          /* Inode number */
    mode_t              v_mode;         /* File mode and type */
    uid_t               v_uid;          /* Owner user ID */
    gid_t               v_gid;          /* Owner group ID */
    off_t               v_size;         /* File size in bytes */
    nlink_t             v_nlink;        /* Number of hard links */
    dev_t               v_rdev;         /* Device ID (for device files) */
    
    time_t              v_atime;        /* Access time */
    time_t              v_mtime;        /* Modification time */
    time_t              v_ctime;        /* Status change time */
    
    uint32_t            v_flags;        /* VNode flags */
    uint32_t            v_refcount;     /* Reference count */
    
    struct vfs_mount    *v_mount;       /* Mounted filesystem */
    struct vnode_operations *v_ops;     /* VNode operations */
    void                *v_private;     /* Filesystem-specific data */
    
    spinlock_t          v_lock;         /* VNode lock */
    struct vnode        *v_next;        /* Next vnode in hash */
} vnode_t;

#define VNODE_FLAG_DIRTY    0x0001      /* Vnode has been modified */
#define VNODE_FLAG_LOCKED   0x0002      /* Vnode is locked */
#define VNODE_FLAG_ROOT     0x0004      /* This is a root vnode */

typedef struct vnode_operations {
    /* File operations */
    ssize_t (*read)(vnode_t *vnode, void *buf, size_t count, off_t offset);
    ssize_t (*write)(vnode_t *vnode, const void *buf, size_t count, off_t offset);
    int (*truncate)(vnode_t *vnode, off_t length);
    
    /* Directory operations */
    int (*readdir)(vnode_t *vnode, vfs_dirent_t *dirent, off_t *offset);
    int (*lookup)(vnode_t *dir, const char *name, vnode_t **result);
    int (*create)(vnode_t *dir, const char *name, mode_t mode, vnode_t **result);
    int (*mkdir)(vnode_t *dir, const char *name, mode_t mode);
    int (*rmdir)(vnode_t *dir, const char *name);
    int (*unlink)(vnode_t *dir, const char *name);
    int (*link)(vnode_t *dir, const char *name, vnode_t *target);
    int (*symlink)(vnode_t *dir, const char *name, const char *target);
    int (*readlink)(vnode_t *vnode, char *buf, size_t bufsize);
    
    /* Attribute operations */
    int (*getattr)(vnode_t *vnode, vfs_stat_t *stat);
    int (*setattr)(vnode_t *vnode, vfs_stat_t *stat);
    
    /* Lifecycle */
    int (*sync)(vnode_t *vnode);
    void (*release)(vnode_t *vnode);
} vnode_ops_t;

typedef struct vfs_mount {
    char                *mnt_point;     /* Mount point path */
    char                *mnt_device;    /* Device name (optional) */
    uint32_t            mnt_flags;      /* Mount flags */
    
    vnode_t             *mnt_root;      /* Root vnode of this filesystem */
    struct vfs_operations *mnt_ops;     /* Filesystem operations */
    void                *mnt_private;   /* Filesystem-specific data */
    
    spinlock_t          mnt_lock;       /* Mount lock */
    struct vfs_mount    *mnt_next;      /* Next mount in list */
} vfs_mount_t;

#define VFS_MNT_RDONLY      0x0001      /* Read-only mount */
#define VFS_MNT_NOSUID      0x0002      /* Ignore suid/sgid bits */
#define VFS_MNT_NOEXEC      0x0004      /* Disallow program execution */
#define VFS_MNT_NODEV       0x0008      /* Disallow device files */

typedef struct vfs_operations {
    const char *fs_name;                /* Filesystem type name */
    
    /* Mount/unmount */
    int (*mount)(const char *device, const char *mountpoint, 
                 uint32_t flags, void *data, vfs_mount_t **mnt);
    int (*unmount)(vfs_mount_t *mnt, uint32_t flags);
    
    /* Filesystem-wide operations */
    int (*statfs)(vfs_mount_t *mnt, void *statbuf);
    int (*sync)(vfs_mount_t *mnt);
    
    /* VNode allocation */
    int (*alloc_vnode)(vfs_mount_t *mnt, ino_t ino, vnode_t **vnode);
    void (*free_vnode)(vnode_t *vnode);
} vfs_ops_t;

typedef struct vfs_file {
    vnode_t     *f_vnode;       /* Associated vnode */
    off_t       f_offset;       /* Current file position */
    uint32_t    f_flags;        /* File flags (O_RDONLY, etc.) */
    uint32_t    f_refcount;     /* Reference count */
    spinlock_t  f_lock;         /* File lock */
} vfs_file_t;

typedef struct vfs_dentry {
    char                *d_name;        /* Component name */
    vnode_t             *d_vnode;       /* Associated vnode */
    struct vfs_dentry   *d_parent;      /* Parent dentry */
    struct vfs_dentry   *d_next;        /* Next in hash chain */
    uint32_t            d_hash;         /* Hash value */
    spinlock_t          d_lock;         /* Dentry lock */
} vfs_dentry_t;

int vfs_init(void);

int vfs_register_filesystem(vfs_ops_t *ops);
int vfs_unregister_filesystem(const char *fs_name);

int vfs_mount(const char *device, const char *mountpoint, 
              const char *fstype, uint32_t flags, void *data);
int vfs_unmount(const char *mountpoint, uint32_t flags);

vnode_t *vfs_vnode_alloc(vfs_mount_t *mnt, ino_t ino);
void vfs_vnode_free(vnode_t *vnode);
void vfs_vnode_ref(vnode_t *vnode);
void vfs_vnode_unref(vnode_t *vnode);

int vfs_lookup(const char *path, vnode_t **result);
static int vfs_lookup_internal(const char *path, vnode_t **result, bool follow_final);
int vfs_lookup_parent(const char *path, vnode_t **parent, char **name);

int vfs_open(const char *path, uint32_t flags, mode_t mode, vfs_file_t **file);
int vfs_close(vfs_file_t *file);
ssize_t vfs_read(vfs_file_t *file, void *buf, size_t count);
ssize_t vfs_write(vfs_file_t *file, const void *buf, size_t count);
off_t vfs_seek(vfs_file_t *file, off_t offset, int whence);
int vfs_stat(const char *path, vfs_stat_t *stat);
int vfs_fstat(vfs_file_t *file, vfs_stat_t *stat);
int vfs_lstat(const char *path, vfs_stat_t *stat);

int vfs_access(const char *path, int mode);
int vfs_chmod(const char *path, mode_t mode);
int vfs_chown(const char *path, uid_t uid, gid_t gid);
int vfs_rename(const char *oldpath, const char *newpath);

int vfs_mkdir(const char *path, mode_t mode);
int vfs_rmdir(const char *path);
int vfs_readdir(vfs_file_t *file, vfs_dirent_t *dirent);

int vfs_create(const char *path, mode_t mode);
int vfs_unlink(const char *path);
int vfs_link(const char *oldpath, const char *newpath);
int vfs_symlink(const char *target, const char *linkpath);
int vfs_readlink(const char *path, char *buf, size_t bufsize);
int vfs_truncate(const char *path, off_t length);

int vfs_sync(void);
int vfs_fsync(vfs_file_t *file);

int vfs_is_absolute_path(const char *path);
int vfs_normalize_path(const char *path, char *normalized);
const char *vfs_basename(const char *path);
const char *vfs_dirname(const char *path, char *buf, size_t bufsize);

vfs_dentry_t *vfs_dentry_lookup(const char *path);
int vfs_dentry_add(const char *path, vnode_t *vnode);
void vfs_dentry_remove(const char *path);
void vfs_dentry_purge(void);

void vfs_dump_mounts(void);
void vfs_dump_vnodes(void);

__END_DECLS

#endif