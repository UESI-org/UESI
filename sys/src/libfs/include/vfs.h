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

#define VFS_SUCCESS 0

#define VFS_IFMT 0170000
#define VFS_IFIFO 0010000
#define VFS_IFCHR 0020000
#define VFS_IFDIR 0040000
#define VFS_IFBLK 0060000
#define VFS_IFREG 0100000
#define VFS_IFLNK 0120000
#define VFS_IFSOCK 0140000

#define VFS_O_RDONLY 0x0000
#define VFS_O_WRONLY 0x0001
#define VFS_O_RDWR 0x0002
#define VFS_O_APPEND 0x0008
#define VFS_O_CREAT 0x0200
#define VFS_O_TRUNC 0x0400
#define VFS_O_EXCL 0x0800

#define VFS_SEEK_SET 0
#define VFS_SEEK_CUR 1
#define VFS_SEEK_END 2

#define VFS_MAX_PATH 4096
#define VFS_MAX_NAME 256
#define VFS_MAX_SYMLINK_DEPTH 8

#ifndef F_OK
#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4
#endif

typedef struct vfs_stat {
	dev_t st_dev;
	ino_t st_ino;
	mode_t st_mode;
	nlink_t st_nlink;
	uid_t st_uid;
	gid_t st_gid;
	dev_t st_rdev;
	off_t st_size;
	blksize_t st_blksize;
	blkcnt_t st_blocks;
	time_t st_atime;
	time_t st_mtime;
	time_t st_ctime;
} vfs_stat_t;

typedef struct vfs_dirent {
	ino_t d_ino;
	off_t d_off;
	uint16_t d_reclen;
	uint8_t d_type;
	char d_name[VFS_MAX_NAME];
} vfs_dirent_t;

typedef struct vnode {
	ino_t v_ino;
	mode_t v_mode;
	uid_t v_uid;
	gid_t v_gid;
	off_t v_size;
	nlink_t v_nlink;
	dev_t v_rdev;
	time_t v_atime;
	time_t v_mtime;
	time_t v_ctime;
	uint32_t v_flags;
	uint32_t v_refcount;
	struct vfs_mount *v_mount;
	struct vnode_operations *v_ops;
	void *v_private;
	spinlock_t v_lock;
	struct vnode *v_next;
} vnode_t;

#define VNODE_FLAG_DIRTY 0x0001
#define VNODE_FLAG_LOCKED 0x0002
#define VNODE_FLAG_ROOT 0x0004

typedef struct vnode_operations {
	ssize_t (*read)(vnode_t *vnode, void *buf, size_t count, off_t offset);
	ssize_t (*write)(vnode_t *vnode,
	                 const void *buf,
	                 size_t count,
	                 off_t offset);
	int (*truncate)(vnode_t *vnode, off_t length);
	int (*readdir)(vnode_t *vnode, vfs_dirent_t *dirent, off_t *offset);
	int (*lookup)(vnode_t *dir, const char *name, vnode_t **result);
	int (*create)(vnode_t *dir,
	              const char *name,
	              mode_t mode,
	              vnode_t **result);
	int (*mkdir)(vnode_t *dir, const char *name, mode_t mode);
	int (*rmdir)(vnode_t *dir, const char *name);
	int (*unlink)(vnode_t *dir, const char *name);
	int (*link)(vnode_t *dir, const char *name, vnode_t *target);
	int (*symlink)(vnode_t *dir, const char *name, const char *target);
	int (*readlink)(vnode_t *vnode, char *buf, size_t bufsize);
	int (*getattr)(vnode_t *vnode, vfs_stat_t *stat);
	int (*setattr)(vnode_t *vnode, vfs_stat_t *stat);
	int (*sync)(vnode_t *vnode);
	void (*release)(vnode_t *vnode);
} vnode_ops_t;

typedef struct vfs_mount {
	char *mnt_point;
	char *mnt_device;
	uint32_t mnt_flags;
	vnode_t *mnt_root;
	struct vfs_operations *mnt_ops;
	void *mnt_private;
	spinlock_t mnt_lock;
	struct vfs_mount *mnt_next;
} vfs_mount_t;

#define VFS_MNT_RDONLY 0x0001
#define VFS_MNT_NOSUID 0x0002
#define VFS_MNT_NOEXEC 0x0004
#define VFS_MNT_NODEV 0x0008

typedef struct vfs_operations {
	const char *fs_name;
	int (*mount)(const char *device,
	             const char *mountpoint,
	             uint32_t flags,
	             void *data,
	             vfs_mount_t **mnt);
	int (*unmount)(vfs_mount_t *mnt, uint32_t flags);
	int (*statfs)(vfs_mount_t *mnt, void *statbuf);
	int (*sync)(vfs_mount_t *mnt);
	int (*alloc_vnode)(vfs_mount_t *mnt, ino_t ino, vnode_t **vnode);
	void (*free_vnode)(vnode_t *vnode);
} vfs_ops_t;

typedef struct vfs_file {
	vnode_t *f_vnode;
	off_t f_offset;
	uint32_t f_flags;
	uint32_t f_refcount;
	spinlock_t f_lock;
} vfs_file_t;

typedef struct vfs_dentry {
	char *d_name;
	vnode_t *d_vnode;
	struct vfs_dentry *d_parent;
	struct vfs_dentry *d_next;
	uint32_t d_hash;
	spinlock_t d_lock;
} vfs_dentry_t;

typedef struct vfs_context {
	vnode_t *cwd;
	vnode_t *root;
	spinlock_t lock;
	mode_t umask;
} vfs_context_t;

typedef struct vfs_stats {
	uint64_t lookups;
	uint64_t cache_hits;
	uint64_t cache_misses;
	uint64_t reads;
	uint64_t writes;
	uint64_t creates;
	uint64_t deletes;
} vfs_stats_t;

int vfs_init(void);

int vfs_register_filesystem(vfs_ops_t *ops);
int vfs_unregister_filesystem(const char *fs_name);

int vfs_mount(const char *device,
              const char *mountpoint,
              const char *fstype,
              uint32_t flags,
              void *data);
int vfs_unmount(const char *mountpoint, uint32_t flags);
vfs_mount_t *vfs_find_mount(const char *path);

vnode_t *vfs_vnode_alloc(vfs_mount_t *mnt, ino_t ino);
void vfs_vnode_free(vnode_t *vnode);
void vfs_vnode_ref(vnode_t *vnode);
void vfs_vnode_unref(vnode_t *vnode);

int vfs_lookup(const char *path, vnode_t **result);
int vfs_lookup_internal(const char *path, vnode_t **result, bool follow_final);
int vfs_lookup_parent(const char *path, vnode_t **parent, char **name);

int vfs_open(const char *path, uint32_t flags, mode_t mode, vfs_file_t **file);
int vfs_close(vfs_file_t *file);
ssize_t vfs_read(vfs_file_t *file, void *buf, size_t count);
ssize_t vfs_write(vfs_file_t *file, const void *buf, size_t count);
off_t vfs_seek(vfs_file_t *file, off_t offset, int whence);
vfs_file_t *vfs_file_dup(vfs_file_t *file);

int vfs_stat(const char *path, vfs_stat_t *stat);
int vfs_fstat(vfs_file_t *file, vfs_stat_t *stat);
int vfs_lstat(const char *path, vfs_stat_t *stat);

int vfs_access(const char *path, int mode);
int vfs_chmod(const char *path, mode_t mode);
int vfs_chown(const char *path, uid_t uid, gid_t gid);
int vfs_rename(const char *oldpath, const char *newpath);

int vfs_mkdir(const char *path, mode_t mode);
int vfs_mknod(const char *path, mode_t mode, dev_t dev);
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
bool vfs_is_valid_filename(const char *name);
bool vfs_path_is_under(const char *path, const char *root);

vfs_dentry_t *vfs_dentry_lookup(const char *path);
int vfs_dentry_add(const char *path, vnode_t *vnode);
void vfs_dentry_remove(const char *path);
void vfs_dentry_purge(void);

vfs_context_t *vfs_context_create(void);
vfs_context_t *vfs_context_dup(vfs_context_t *ctx);
void vfs_context_free(vfs_context_t *ctx);
int vfs_chdir(vfs_context_t *ctx, const char *path);
int vfs_getcwd(vfs_context_t *ctx, char *buf, size_t size);

void vfs_get_stats(vfs_stats_t *stats);
void vfs_reset_stats(void);
void vfs_print_stats(void);

void vfs_dump_mounts(void);
void vfs_dump_vnodes(void);

__END_DECLS

#endif
