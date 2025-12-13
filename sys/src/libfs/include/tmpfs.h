#ifndef _FS_TMPFS_H_
#define _FS_TMPFS_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <vfs.h>
#include <stdint.h>
#include <stdbool.h>

__BEGIN_DECLS

#define TMPFS_MAGIC         0x54504653  /* "TPFS" */
#define TMPFS_DEFAULT_MODE  0755
#define TMPFS_ROOT_INO      1

#define TMPFS_FILE      1
#define TMPFS_DIR       2
#define TMPFS_SYMLINK   3

typedef struct tmpfs_mount {
    uint64_t        tm_max_size;        /* Maximum filesystem size */
    uint64_t        tm_used_size;       /* Current used size */
    ino_t           tm_next_ino;        /* Next inode number */
    struct tmpfs_node *tm_root;         /* Root directory node */
    struct vfs_mount *tm_vfs_mount;     /* Back pointer to VFS mount */
    spinlock_t      tm_lock;            /* Mount lock */
} tmpfs_mount_t;

typedef struct tmpfs_dirent {
    char            *td_name;           /* Entry name */
    struct tmpfs_node *td_node;         /* Node pointer */
    struct tmpfs_dirent *td_next;       /* Next entry in directory */
} tmpfs_dirent_t;

typedef struct tmpfs_node {
    ino_t           tn_ino;             /* Inode number */
    uint8_t         tn_type;            /* Node type (FILE/DIR/SYMLINK) */
    mode_t          tn_mode;            /* File mode and permissions */
    uid_t           tn_uid;             /* Owner user ID */
    gid_t           tn_gid;             /* Owner group ID */
    nlink_t         tn_nlink;           /* Number of hard links */
    off_t           tn_size;            /* File size */
    
    time_t          tn_atime;           /* Access time */
    time_t          tn_mtime;           /* Modification time */
    time_t          tn_ctime;           /* Status change time */
    
    union {
        /* For files: data buffer */
        struct {
            void        *data;          /* File data */
            size_t      capacity;       /* Allocated capacity */
        } file;
        
        /* For directories: list of entries */
        struct {
            tmpfs_dirent_t  *entries;   /* Directory entries */
            uint32_t        count;      /* Number of entries */
        } dir;
        
        /* For symlinks: target path */
        struct {
            char        *target;        /* Symlink target */
        } symlink;
    } tn_data;
    
    struct vfs_mount *tn_mount;         /* Mount point (VFS mount) */
    spinlock_t      tn_lock;            /* Node lock */
} tmpfs_node_t;

int tmpfs_init(void);

int tmpfs_mount(const char *device, const char *mountpoint,
                uint32_t flags, void *data, vfs_mount_t **mnt);
int tmpfs_unmount(vfs_mount_t *mnt, uint32_t flags);
int tmpfs_statfs(vfs_mount_t *mnt, void *statbuf);
int tmpfs_sync(vfs_mount_t *mnt);

tmpfs_node_t *tmpfs_node_alloc(tmpfs_mount_t *tm, uint8_t type, mode_t mode);
void tmpfs_node_free(tmpfs_node_t *node);

ssize_t tmpfs_read(vnode_t *vnode, void *buf, size_t count, off_t offset);
ssize_t tmpfs_write(vnode_t *vnode, const void *buf, size_t count, off_t offset);
int tmpfs_truncate(vnode_t *vnode, off_t length);
int tmpfs_readdir(vnode_t *vnode, vfs_dirent_t *dirent, off_t *offset);
int tmpfs_lookup(vnode_t *dir, const char *name, vnode_t **result);
int tmpfs_create(vnode_t *dir, const char *name, mode_t mode, vnode_t **result);
int tmpfs_mkdir(vnode_t *dir, const char *name, mode_t mode);
int tmpfs_rmdir(vnode_t *dir, const char *name);
int tmpfs_unlink(vnode_t *dir, const char *name);
int tmpfs_link(vnode_t *dir, const char *name, vnode_t *target);
int tmpfs_symlink(vnode_t *dir, const char *name, const char *target);
int tmpfs_readlink(vnode_t *vnode, char *buf, size_t bufsize);
int tmpfs_getattr(vnode_t *vnode, vfs_stat_t *stat);
int tmpfs_setattr(vnode_t *vnode, vfs_stat_t *stat);
void tmpfs_release(vnode_t *vnode);

tmpfs_node_t *tmpfs_vnode_to_node(vnode_t *vnode);
int tmpfs_node_to_vnode(tmpfs_node_t *node, vnode_t **vnode);
tmpfs_dirent_t *tmpfs_dirent_find(tmpfs_node_t *dir, const char *name);
int tmpfs_dirent_add(tmpfs_node_t *dir, const char *name, tmpfs_node_t *node);
int tmpfs_dirent_remove(tmpfs_node_t *dir, const char *name);

__END_DECLS

#endif