#include <sys/time.h>
#include <sys/types.h>
#include <printf.h>
#include <vfs.h>
#include <errno.h>
#include <sys/panic.h>
#include <kmalloc.h>
#include <kfree.h>
#include <string.h>

extern uint64_t tsc_get_time_ns(void);
extern void serial_printf(uint16_t port, const char *fmt, ...);
#define DEBUG_PORT 0x3F8
#define VFS_DEBUG(fmt, ...) serial_printf(DEBUG_PORT, "[VFS] " fmt, ##__VA_ARGS__)

static vfs_mount_t *mount_list = NULL;
static spinlock_t mount_list_lock = SPINLOCK_INITIALIZER("mount_list");

typedef struct fs_type_node {
	struct vfs_operations *ops;
	struct fs_type_node *next;
} fs_type_node_t;

static fs_type_node_t *fs_types = NULL;
static spinlock_t fs_types_lock = SPINLOCK_INITIALIZER("fs_types");

#define VNODE_HASH_SIZE 256
static vnode_t *vnode_hash[VNODE_HASH_SIZE];
static spinlock_t vnode_hash_locks[VNODE_HASH_SIZE];

#define DENTRY_HASH_SIZE 512
static vfs_dentry_t *dentry_hash[DENTRY_HASH_SIZE];
static spinlock_t dentry_hash_lock = SPINLOCK_INITIALIZER("dentry_hash");

static vfs_mount_t *root_mount = NULL;

static vfs_stats_t vfs_stats = {0};
static spinlock_t vfs_stats_lock = SPINLOCK_INITIALIZER("vfs_stats");

static inline uint32_t
vnode_hash_func(dev_t dev, ino_t ino)
{
	return ((uint32_t)dev ^ (uint32_t)ino) % VNODE_HASH_SIZE;
}

static uint32_t
dentry_hash_func(const char *path)
{
	uint32_t hash = 5381;
	int c;
	while ((c = *path++))
		hash = ((hash << 5) + hash) + c;
	return hash % DENTRY_HASH_SIZE;
}

static char *
vfs_strdup(const char *str)
{
	if (str == NULL)
		return NULL;
	size_t len = strlen(str) + 1;
	char *dup = kmalloc(len);
	if (dup != NULL)
		memcpy(dup, str, len);
	return dup;
}

static inline time_t
vfs_now(void)
{
	static time_t last;
	time_t now = (time_t)(tsc_get_time_ns() / 1000000000ULL);

	if (now < last)
		now = last;
	last = now;
	return now;
}

static void
vfs_update_atime(vnode_t *vn)
{
	if (!vn)
		return;

	time_t now = vfs_now();

	if (vn->v_atime < vn->v_mtime || vn->v_atime + 60 < now)
    	    vn->v_atime = now;
}

static void
vfs_update_mtime(vnode_t *vn)
{
	if (!vn)
		return;

	time_t now = vfs_now();
	vn->v_mtime = now;
	vn->v_ctime = now;
}

static void
vfs_stats_record_lookup(bool cache_hit)
{
	uint64_t flags;
	spinlock_acquire_irqsave(&vfs_stats_lock, &flags);
	vfs_stats.lookups++;
	if (cache_hit)
		vfs_stats.cache_hits++;
	else
		vfs_stats.cache_misses++;
	spinlock_release_irqrestore(&vfs_stats_lock, flags);
}

int
vfs_init(void)
{
	VFS_DEBUG("Initializing Virtual File System\n");

	memset(vnode_hash, 0, sizeof(vnode_hash));
	memset(dentry_hash, 0, sizeof(dentry_hash));

	for (int i = 0; i < VNODE_HASH_SIZE; i++) {
		char lock_name[32];
		snprintf(lock_name, sizeof(lock_name), "vnode_hash_%d", i);
		spinlock_init(&vnode_hash_locks[i], lock_name);
	}

	mount_list = NULL;
	fs_types = NULL;
	root_mount = NULL;

	VFS_DEBUG("VFS initialization complete\n");
	return VFS_SUCCESS;
}

int
vfs_register_filesystem(vfs_ops_t *ops)
{
	if (ops == NULL || ops->fs_name == NULL)
		return -EINVAL;

	uint64_t flags;
	spinlock_acquire_irqsave(&fs_types_lock, &flags);

	fs_type_node_t *current = fs_types;
	while (current != NULL) {
		if (strcmp(current->ops->fs_name, ops->fs_name) == 0) {
			spinlock_release_irqrestore(&fs_types_lock, flags);
			VFS_DEBUG("Filesystem '%s' already registered\n", ops->fs_name);
			return -EEXIST;
		}
		current = current->next;
	}

	fs_type_node_t *node = kmalloc(sizeof(fs_type_node_t));
	if (node == NULL) {
		spinlock_release_irqrestore(&fs_types_lock, flags);
		return -ENOMEM;
	}

	node->ops = ops;
	node->next = fs_types;
	fs_types = node;

	spinlock_release_irqrestore(&fs_types_lock, flags);

	VFS_DEBUG("Registered filesystem: %s\n", ops->fs_name);
	return VFS_SUCCESS;
}

int
vfs_unregister_filesystem(const char *fs_name)
{
	if (fs_name == NULL)
		return -EINVAL;

	uint64_t flags;
	spinlock_acquire_irqsave(&fs_types_lock, &flags);

	fs_type_node_t **current = &fs_types;
	while (*current != NULL) {
		if (strcmp((*current)->ops->fs_name, fs_name) == 0) {
			fs_type_node_t *to_free = *current;
			*current = (*current)->next;
			kfree(to_free);
			spinlock_release_irqrestore(&fs_types_lock, flags);
			VFS_DEBUG("Unregistered filesystem: %s\n", fs_name);
			return VFS_SUCCESS;
		}
		current = &(*current)->next;
	}

	spinlock_release_irqrestore(&fs_types_lock, flags);
	return -ENOENT;
}

static vfs_ops_t *
vfs_find_filesystem(const char *fs_name)
{
	uint64_t flags;
	spinlock_acquire_irqsave(&fs_types_lock, &flags);

	fs_type_node_t *current = fs_types;
	while (current != NULL) {
		if (strcmp(current->ops->fs_name, fs_name) == 0) {
			spinlock_release_irqrestore(&fs_types_lock, flags);
			return current->ops;
		}
		current = current->next;
	}

	spinlock_release_irqrestore(&fs_types_lock, flags);
	return NULL;
}

int
vfs_mount(const char *device, const char *mountpoint, const char *fstype, uint32_t flags, void *data)
{
	if (mountpoint == NULL || fstype == NULL)
		return -EINVAL;

	bool is_root = (strcmp(mountpoint, "/") == 0);

	vfs_ops_t *ops = vfs_find_filesystem(fstype);
	if (ops == NULL) {
		VFS_DEBUG("Unknown filesystem type: %s\n", fstype);
		return -ENODEV;
	}

	if (!is_root && root_mount != NULL) {
		vnode_t *mp_vnode;
		int ret = vfs_lookup(mountpoint, &mp_vnode);
		if (ret != 0) {
			VFS_DEBUG("Mount point %s does not exist\n", mountpoint);
			return -ENOENT;
		}

		if ((mp_vnode->v_mode & VFS_IFMT) != VFS_IFDIR) {
			vfs_vnode_unref(mp_vnode);
			VFS_DEBUG("Mount point %s is not a directory\n", mountpoint);
			return -ENOTDIR;
		}

		vfs_vnode_unref(mp_vnode);
	}

	if (ops->mount == NULL)
		return -ENOSYS;

	vfs_mount_t *mnt = NULL;
	int ret = ops->mount(device, mountpoint, flags, data, &mnt);
	if (ret != 0) {
		VFS_DEBUG("Mount failed: %d\n", ret);
		return ret;
	}

	uint64_t irq_flags;
	spinlock_acquire_irqsave(&mount_list_lock, &irq_flags);

	mnt->mnt_next = mount_list;
	mount_list = mnt;

	if (root_mount == NULL && is_root) {
		root_mount = mnt;
		VFS_DEBUG("Set root mount\n");
	}

	spinlock_release_irqrestore(&mount_list_lock, irq_flags);

	VFS_DEBUG("Mounted %s at %s (type: %s)\n",
	          device ? device : "(none)", mountpoint, fstype);

	return VFS_SUCCESS;
}

int
vfs_unmount(const char *mountpoint, uint32_t flags)
{
	if (mountpoint == NULL)
		return -EINVAL;

	uint64_t irq_flags;
	spinlock_acquire_irqsave(&mount_list_lock, &irq_flags);

	vfs_mount_t **current = &mount_list;
	while (*current != NULL) {
		if (strcmp((*current)->mnt_point, mountpoint) == 0) {
			vfs_mount_t *mnt = *current;

			if (mnt == root_mount) {
				spinlock_release_irqrestore(&mount_list_lock, irq_flags);
				return -EBUSY;
			}

			if (mnt->mnt_ops && mnt->mnt_ops->unmount) {
				int ret = mnt->mnt_ops->unmount(mnt, flags);
				if (ret != 0) {
					spinlock_release_irqrestore(&mount_list_lock, irq_flags);
					return ret;
				}
			}

			*current = mnt->mnt_next;

			if (mnt->mnt_point)
				kfree(mnt->mnt_point);
			if (mnt->mnt_device)
				kfree(mnt->mnt_device);
			kfree(mnt);

			spinlock_release_irqrestore(&mount_list_lock, irq_flags);
			VFS_DEBUG("Unmounted %s\n", mountpoint);
			return VFS_SUCCESS;
		}
		current = &(*current)->mnt_next;
	}

	spinlock_release_irqrestore(&mount_list_lock, irq_flags);
	return -ENOENT;
}

vfs_mount_t *
vfs_find_mount(const char *path)
{
	if (path == NULL)
		return NULL;

	char normalized[VFS_MAX_PATH];
	if (vfs_normalize_path(path, normalized) != 0)
		return NULL;

	uint64_t flags;
	spinlock_acquire_irqsave(&mount_list_lock, &flags);

	vfs_mount_t *best_match = root_mount;
	size_t best_len = 0;

	vfs_mount_t *mnt = mount_list;
	while (mnt != NULL) {
		if (mnt->mnt_point == NULL) {
			mnt = mnt->mnt_next;
			continue;
		}

		size_t mnt_len = strlen(mnt->mnt_point);
		
		if (strncmp(normalized, mnt->mnt_point, mnt_len) == 0) {
			if (normalized[mnt_len] == '\0' || 
			    normalized[mnt_len] == '/' ||
			    strcmp(mnt->mnt_point, "/") == 0) {
				if (mnt_len > best_len) {
					best_match = mnt;
					best_len = mnt_len;
				}
			}
		}

		mnt = mnt->mnt_next;
	}

	spinlock_release_irqrestore(&mount_list_lock, flags);
	return best_match;
}

vnode_t *
vfs_vnode_alloc(vfs_mount_t *mnt, ino_t ino)
{
	vnode_t *vnode = kmalloc(sizeof(vnode_t));
	if (vnode == NULL)
		return NULL;

	memset(vnode, 0, sizeof(vnode_t));

	time_t now = vfs_now();
	vnode->v_atime = now;
	vnode->v_mtime = now;
	vnode->v_ctime = now;

	vnode->v_ino = ino;
	vnode->v_mount = mnt;
	vnode->v_refcount = 1;
	spinlock_init(&vnode->v_lock, "vnode");

	uint32_t hash = vnode_hash_func(0, ino);

	uint64_t flags;
	spinlock_acquire_irqsave(&vnode_hash_locks[hash], &flags);
	vnode->v_next = vnode_hash[hash];
	vnode_hash[hash] = vnode;
	spinlock_release_irqrestore(&vnode_hash_locks[hash], flags);

	return vnode;
}

void
vfs_vnode_free(vnode_t *vnode)
{
	if (vnode == NULL)
		return;

	if (vnode->v_ops && vnode->v_ops->release)
		vnode->v_ops->release(vnode);

	kfree(vnode);
}

void
vfs_vnode_ref(vnode_t *vnode)
{
	if (vnode == NULL)
		return;

	uint64_t flags;
	spinlock_acquire_irqsave(&vnode->v_lock, &flags);
	vnode->v_refcount++;
	spinlock_release_irqrestore(&vnode->v_lock, flags);
}

void
vfs_vnode_unref(vnode_t *vnode)
{
	if (vnode == NULL)
		return;

	uint64_t flags;
	spinlock_acquire_irqsave(&vnode->v_lock, &flags);

	if (vnode->v_refcount == 0) {
		VFS_DEBUG("Warning: vnode refcount already 0!\n");
		spinlock_release_irqrestore(&vnode->v_lock, flags);
		return;
	}

	vnode->v_refcount--;

	if (vnode->v_refcount == 0) {
		uint32_t hash = vnode_hash_func(0, vnode->v_ino);
		
		/* Must release vnode lock before acquiring hash lock to prevent deadlock */
		spinlock_release_irqrestore(&vnode->v_lock, flags);

		uint64_t hash_flags;
		spinlock_acquire_irqsave(&vnode_hash_locks[hash], &hash_flags);

		vnode_t **current = &vnode_hash[hash];
		while (*current != NULL) {
			if (*current == vnode) {
				*current = vnode->v_next;
				break;
			}
			current = &(*current)->v_next;
		}

		spinlock_release_irqrestore(&vnode_hash_locks[hash], hash_flags);

		if (vnode->v_ops && vnode->v_ops->release)
			vnode->v_ops->release(vnode);
		kfree(vnode);
		return;
	}

	spinlock_release_irqrestore(&vnode->v_lock, flags);
}

int
vfs_is_absolute_path(const char *path)
{
	return path != NULL && path[0] == '/';
}

int
vfs_normalize_path(const char *path, char *normalized)
{
	if (path == NULL || normalized == NULL)
		return -EINVAL;

	if (!vfs_is_absolute_path(path))
		return -EINVAL;

	size_t len = strlen(path);
	if (len >= VFS_MAX_PATH)
		return -ENAMETOOLONG;

	char *dst = normalized;
	const char *src = path;

	*dst++ = '/';

	while (*src) {
		while (*src == '/')
			src++;

		if (*src == '\0')
			break;

		const char *component_start = src;
		while (*src && *src != '/')
			src++;

		size_t component_len = src - component_start;

		if (component_len == 1 && component_start[0] == '.')
			continue;

		if (component_len == 2 && component_start[0] == '.' && component_start[1] == '.') {
			if (dst > normalized + 1) {
				dst--;
				while (dst > normalized && *(dst - 1) != '/')
					dst--;
			}
			continue;
		}

		if (dst > normalized + 1)
			*dst++ = '/';

		if (dst + component_len >= normalized + VFS_MAX_PATH)
			return -ENAMETOOLONG;

		memcpy(dst, component_start, component_len);
		dst += component_len;
	}

	if (dst == normalized)
		*dst++ = '/';

	if (dst > normalized + 1 && *(dst - 1) == '/')
		dst--;

	*dst = '\0';
	return VFS_SUCCESS;
}

const char *
vfs_basename(const char *path)
{
	if (path == NULL)
		return NULL;

	const char *base = strrchr(path, '/');
	return base ? base + 1 : path;
}

const char *
vfs_dirname(const char *path, char *buf, size_t bufsize)
{
	if (path == NULL || buf == NULL || bufsize == 0)
		return NULL;

	const char *last_slash = strrchr(path, '/');
	if (last_slash == NULL) {
		buf[0] = '.';
		buf[1] = '\0';
		return buf;
	}

	size_t len = last_slash - path;
	if (len == 0)
		len = 1;

	if (len >= bufsize)
		return NULL;

	strncpy(buf, path, len);
	buf[len] = '\0';
	return buf;
}

int
vfs_lookup(const char *path, vnode_t **result)
{
	return vfs_lookup_internal(path, result, true);
}

int
vfs_lookup_internal(const char *path, vnode_t **result, bool follow_final)
{
	if (path == NULL || result == NULL)
		return -EINVAL;

	if (!vfs_is_absolute_path(path))
		return -EINVAL;

	if (root_mount == NULL || root_mount->mnt_root == NULL) {
		VFS_DEBUG("No root filesystem mounted\n");
		return -ENOENT;
	}

	char normalized[VFS_MAX_PATH];
	int ret = vfs_normalize_path(path, normalized);
	if (ret != 0)
		return ret;

	if (follow_final) {
		vfs_dentry_t *dentry = vfs_dentry_lookup(normalized);
		if (dentry != NULL && dentry->d_vnode != NULL) {
			*result = dentry->d_vnode;
			vfs_vnode_ref(*result);
			vfs_update_atime(*result);
			vfs_stats_record_lookup(true);
			return VFS_SUCCESS;
		}
	}

	vfs_stats_record_lookup(false);

	if (strcmp(normalized, "/") == 0) {
		*result = root_mount->mnt_root;
		vfs_vnode_ref(*result);
		return VFS_SUCCESS;
	}

	char *path_copy = kmalloc(strlen(normalized) + 1);
	if (path_copy == NULL)
		return -ENOMEM;
	strcpy(path_copy, normalized);

	char *components[64];
	int component_count = 0;

	char *token = strtok(path_copy + 1, "/");
	while (token != NULL && component_count < 64) {
		components[component_count++] = token;
		token = strtok(NULL, "/");
	}

	if (component_count == 0) {
		kfree(path_copy);
		*result = root_mount->mnt_root;
		vfs_vnode_ref(*result);
		return VFS_SUCCESS;
	}

	vnode_t *current = root_mount->mnt_root;
	vfs_vnode_ref(current);

	int symlink_depth = 0;

	for (int i = 0; i < component_count; i++) {
		bool is_final = (i == component_count - 1);
		char *component = components[i];

		if ((current->v_mode & VFS_IFMT) != VFS_IFDIR) {
			vfs_vnode_unref(current);
			kfree(path_copy);
			return -ENOTDIR;
		}

		if (current->v_ops == NULL || current->v_ops->lookup == NULL) {
			vfs_vnode_unref(current);
			kfree(path_copy);
			return -ENOSYS;
		}

		vnode_t *next = NULL;
		ret = current->v_ops->lookup(current, component, &next);

		vfs_vnode_unref(current);

		if (ret != 0) {
			kfree(path_copy);
			return ret;
		}

		current = next;

		if ((current->v_mode & VFS_IFMT) == VFS_IFLNK) {
			if (is_final && !follow_final)
				break;

			if (symlink_depth >= VFS_MAX_SYMLINK_DEPTH) {
				vfs_vnode_unref(current);
				kfree(path_copy);
				return -ELOOP;
			}
			symlink_depth++;

			char target[VFS_MAX_PATH];
			if (current->v_ops == NULL || current->v_ops->readlink == NULL) {
				vfs_vnode_unref(current);
				kfree(path_copy);
				return -EINVAL;
			}

			ret = current->v_ops->readlink(current, target, sizeof(target));
			if (ret < 0) {
				vfs_vnode_unref(current);
				kfree(path_copy);
				return ret;
			}

			if (ret >= VFS_MAX_PATH) {
				vfs_vnode_unref(current);
				kfree(path_copy);
				return -ENAMETOOLONG;
			}
			target[ret] = '\0';

			vfs_vnode_unref(current);

			if (target[0] == '/') {
				ret = vfs_lookup_internal(target, &current, true);
				if (ret != 0) {
					kfree(path_copy);
					return ret;
				}
			} else {
				char parent_path[VFS_MAX_PATH];
				parent_path[0] = '/';
				parent_path[1] = '\0';
				size_t path_len = 1;

				for (int j = 0; j < i; j++) {
					size_t comp_len = strlen(components[j]);
					
					if (path_len + comp_len + 2 >= VFS_MAX_PATH) {
						kfree(path_copy);
						return -ENAMETOOLONG;
					}

					if (path_len > 1) {
						strcat(parent_path, "/");
						path_len++;
					}
					strcat(parent_path, components[j]);
					path_len += comp_len;
				}

				size_t target_len = strlen(target);
				if (path_len + target_len + 2 >= VFS_MAX_PATH) {
					kfree(path_copy);
					return -ENAMETOOLONG;
				}

				if (path_len > 1)
					strcat(parent_path, "/");
				strcat(parent_path, target);

				ret = vfs_lookup_internal(parent_path, &current, true);
				if (ret != 0) {
					kfree(path_copy);
					return ret;
				}
			}
		}
	}

	kfree(path_copy);
	
	if (follow_final && current != NULL)
		vfs_dentry_add(normalized, current);
	
	*result = current;
	vfs_update_atime(current);
	return VFS_SUCCESS;
}

int
vfs_lookup_parent(const char *path, vnode_t **parent, char **name)
{
	if (path == NULL || parent == NULL || name == NULL)
		return -EINVAL;

	char dir_buf[VFS_MAX_PATH];
	const char *dirname = vfs_dirname(path, dir_buf, sizeof(dir_buf));
	if (dirname == NULL)
		return -ENAMETOOLONG;

	int ret = vfs_lookup(dirname, parent);
	if (ret != 0)
		return ret;

	const char *basename = vfs_basename(path);
	*name = vfs_strdup(basename);
	if (*name == NULL) {
		vfs_vnode_unref(*parent);
		return -ENOMEM;
	}

	return VFS_SUCCESS;
}

int
vfs_open(const char *path, uint32_t flags, mode_t mode, vfs_file_t **file)
{
	if (path == NULL || file == NULL)
		return -EINVAL;

	vnode_t *vnode = NULL;
	int ret = vfs_lookup(path, &vnode);

	if (ret == 0) {
		if ((flags & VFS_O_CREAT) && (flags & VFS_O_EXCL)) {
			vfs_vnode_unref(vnode);
			return -EEXIST;
		}
	} else if (ret == -ENOENT) {
		if (flags & VFS_O_CREAT) {
			ret = vfs_create(path, mode);
			if (ret != 0)
				return ret;

			ret = vfs_lookup(path, &vnode);
			if (ret != 0)
				return ret;
		} else {
			return -ENOENT;
		}
	} else {
		return ret;
	}

	vfs_file_t *f = kmalloc(sizeof(vfs_file_t));
	if (f == NULL) {
		vfs_vnode_unref(vnode);
		return -ENOMEM;
	}

	f->f_vnode = vnode;
	f->f_offset = 0;
	f->f_flags = flags;
	f->f_refcount = 1;
	spinlock_init(&f->f_lock, "file");

	if (flags & VFS_O_TRUNC) {
		if (vnode->v_ops && vnode->v_ops->truncate) {
			ret = vnode->v_ops->truncate(vnode, 0);
			if (ret != 0) {
				kfree(f);
				vfs_vnode_unref(vnode);
				return ret;
			}
		}
	}

	*file = f;
	return VFS_SUCCESS;
}

int
vfs_close(vfs_file_t *file)
{
	if (file == NULL)
		return -EINVAL;

	uint64_t flags;
	spinlock_acquire_irqsave(&file->f_lock, &flags);

	file->f_refcount--;
	if (file->f_refcount > 0) {
		spinlock_release_irqrestore(&file->f_lock, flags);
		return VFS_SUCCESS;
	}

	spinlock_release_irqrestore(&file->f_lock, flags);

	vfs_vnode_unref(file->f_vnode);
	kfree(file);

	return VFS_SUCCESS;
}

ssize_t
vfs_read(vfs_file_t *file, void *buf, size_t count)
{
	if (file == NULL || buf == NULL)
		return -EINVAL;

	if (!(file->f_flags & (VFS_O_RDONLY | VFS_O_RDWR)))
		return -EBADF;

	vnode_t *vnode = file->f_vnode;
	if (vnode == NULL || vnode->v_ops == NULL || vnode->v_ops->read == NULL)
		return -ENOSYS;

	uint64_t flags;
	spinlock_acquire_irqsave(&file->f_lock, &flags);
	off_t offset = file->f_offset;
	spinlock_release_irqrestore(&file->f_lock, flags);

	ssize_t bytes = vnode->v_ops->read(vnode, buf, count, offset);

	if (bytes > 0) {
		spinlock_acquire_irqsave(&file->f_lock, &flags);
		file->f_offset = offset + bytes;
		spinlock_release_irqrestore(&file->f_lock, flags);
		
		vfs_update_atime(vnode);
		
		uint64_t stat_flags;
		spinlock_acquire_irqsave(&vfs_stats_lock, &stat_flags);
		vfs_stats.reads++;
		spinlock_release_irqrestore(&vfs_stats_lock, stat_flags);
	}

	return bytes;
}

ssize_t
vfs_write(vfs_file_t *file, const void *buf, size_t count)
{
	if (file == NULL || buf == NULL)
		return -EINVAL;

	if (!(file->f_flags & (VFS_O_WRONLY | VFS_O_RDWR)))
		return -EBADF;

	vnode_t *vnode = file->f_vnode;
	if (vnode == NULL || vnode->v_ops == NULL || vnode->v_ops->write == NULL)
		return -ENOSYS;

	off_t offset;
	uint64_t flags;

	if (file->f_flags & VFS_O_APPEND) {
		/* For append mode, must lock vnode to safely read size */
		uint64_t vnode_flags;
		spinlock_acquire_irqsave(&vnode->v_lock, &vnode_flags);
		offset = vnode->v_size;
		spinlock_release_irqrestore(&vnode->v_lock, vnode_flags);
	} else {
		spinlock_acquire_irqsave(&file->f_lock, &flags);
		offset = file->f_offset;
		spinlock_release_irqrestore(&file->f_lock, flags);
	}

	ssize_t bytes = vnode->v_ops->write(vnode, buf, count, offset);

	if (bytes > 0) {
		spinlock_acquire_irqsave(&file->f_lock, &flags);
		file->f_offset = offset + bytes;
		spinlock_release_irqrestore(&file->f_lock, flags);

		vfs_update_mtime(vnode);
		
		uint64_t stat_flags;
		spinlock_acquire_irqsave(&vfs_stats_lock, &stat_flags);
		vfs_stats.writes++;
		spinlock_release_irqrestore(&vfs_stats_lock, stat_flags);
	}

	return bytes;
}

off_t
vfs_seek(vfs_file_t *file, off_t offset, int whence)
{
	if (file == NULL)
		return -EINVAL;

	uint64_t flags;
	spinlock_acquire_irqsave(&file->f_lock, &flags);

	off_t new_offset;

	switch (whence) {
	case VFS_SEEK_SET:
		new_offset = offset;
		break;
	case VFS_SEEK_CUR:
		new_offset = file->f_offset + offset;
		break;
	case VFS_SEEK_END:
		new_offset = file->f_vnode->v_size + offset;
		break;
	default:
		spinlock_release_irqrestore(&file->f_lock, flags);
		return -EINVAL;
	}

	if (new_offset < 0) {
		spinlock_release_irqrestore(&file->f_lock, flags);
		return -EINVAL;
	}

	file->f_offset = new_offset;
	spinlock_release_irqrestore(&file->f_lock, flags);

	return new_offset;
}

vfs_file_t *
vfs_file_dup(vfs_file_t *file)
{
	if (file == NULL)
		return NULL;

	uint64_t flags;
	spinlock_acquire_irqsave(&file->f_lock, &flags);
	file->f_refcount++;
	spinlock_release_irqrestore(&file->f_lock, flags);

	return file;
}

int
vfs_stat(const char *path, vfs_stat_t *stat)
{
	if (path == NULL || stat == NULL)
		return -EINVAL;

	vnode_t *vnode;
	int ret = vfs_lookup(path, &vnode);
	if (ret != 0)
		return ret;

	if (vnode->v_ops && vnode->v_ops->getattr) {
		ret = vnode->v_ops->getattr(vnode, stat);
	} else {
		memset(stat, 0, sizeof(vfs_stat_t));
		stat->st_ino = vnode->v_ino;
		stat->st_mode = vnode->v_mode;
		stat->st_nlink = vnode->v_nlink;
		stat->st_uid = vnode->v_uid;
		stat->st_gid = vnode->v_gid;
		stat->st_size = vnode->v_size;
		stat->st_atime = vnode->v_atime;
		stat->st_mtime = vnode->v_mtime;
		stat->st_ctime = vnode->v_ctime;
		ret = VFS_SUCCESS;
	}

	vfs_vnode_unref(vnode);
	return ret;
}

int
vfs_fstat(vfs_file_t *file, vfs_stat_t *stat)
{
	if (file == NULL || stat == NULL)
		return -EINVAL;

	vnode_t *vnode = file->f_vnode;

	if (vnode->v_ops && vnode->v_ops->getattr)
		return vnode->v_ops->getattr(vnode, stat);

	memset(stat, 0, sizeof(vfs_stat_t));
	stat->st_ino = vnode->v_ino;
	stat->st_mode = vnode->v_mode;
	stat->st_nlink = vnode->v_nlink;
	stat->st_uid = vnode->v_uid;
	stat->st_gid = vnode->v_gid;
	stat->st_size = vnode->v_size;
	stat->st_atime = vnode->v_atime;
	stat->st_mtime = vnode->v_mtime;
	stat->st_ctime = vnode->v_ctime;

	return VFS_SUCCESS;
}

int
vfs_lstat(const char *path, vfs_stat_t *stat)
{
	if (path == NULL || stat == NULL)
		return -EINVAL;

	vnode_t *vnode;
	int ret = vfs_lookup_internal(path, &vnode, false);
	if (ret != 0)
		return ret;

	if (vnode->v_ops && vnode->v_ops->getattr) {
		ret = vnode->v_ops->getattr(vnode, stat);
	} else {
		memset(stat, 0, sizeof(vfs_stat_t));
		stat->st_ino = vnode->v_ino;
		stat->st_mode = vnode->v_mode;
		stat->st_nlink = vnode->v_nlink;
		stat->st_uid = vnode->v_uid;
		stat->st_gid = vnode->v_gid;
		stat->st_size = vnode->v_size;
		stat->st_atime = vnode->v_atime;
		stat->st_mtime = vnode->v_mtime;
		stat->st_ctime = vnode->v_ctime;
		ret = VFS_SUCCESS;
	}

	vfs_vnode_unref(vnode);
	return ret;
}

int
vfs_access(const char *path, int mode)
{
	vnode_t *vnode;
	int ret = vfs_lookup(path, &vnode);
	if (ret != 0)
		return ret;

	uid_t uid = 0; /* TODO: get from current process */
	gid_t gid = 0; /* TODO: get from current process */

	if (uid == 0) {
		vfs_vnode_unref(vnode);
		return VFS_SUCCESS;
	}

	mode_t file_mode = vnode->v_mode;
	mode_t req_mode = 0;

	if (mode & R_OK)
		req_mode |= 0444;
	if (mode & W_OK)
		req_mode |= 0222;
	if (mode & X_OK)
		req_mode |= 0111;

	if (vnode->v_uid == uid) {
		mode_t owner_perms = (file_mode >> 6) & 07;
		mode_t req_owner = (req_mode >> 6) & 07;
		if ((owner_perms & req_owner) == req_owner) {
			vfs_vnode_unref(vnode);
			return VFS_SUCCESS;
		}
		vfs_vnode_unref(vnode);
		return -EACCES;
	}

	if (vnode->v_gid == gid) {
		mode_t group_perms = (file_mode >> 3) & 07;
		mode_t req_group = (req_mode >> 3) & 07;
		if ((group_perms & req_group) == req_group) {
			vfs_vnode_unref(vnode);
			return VFS_SUCCESS;
		}
		vfs_vnode_unref(vnode);
		return -EACCES;
	}

	mode_t other_perms = file_mode & 07;
	mode_t req_other = req_mode & 07;
	if ((other_perms & req_other) == req_other) {
		vfs_vnode_unref(vnode);
		return VFS_SUCCESS;
	}

	vfs_vnode_unref(vnode);
	return -EACCES;
}

int
vfs_chmod(const char *path, mode_t mode)
{
	vnode_t *vnode;
	int ret = vfs_lookup(path, &vnode);
	if (ret != 0)
		return ret;

	uint64_t flags;
	spinlock_acquire_irqsave(&vnode->v_lock, &flags);
	vnode->v_mode = (vnode->v_mode & VFS_IFMT) | (mode & ~VFS_IFMT);
	vnode->v_flags |= VNODE_FLAG_DIRTY;
	spinlock_release_irqrestore(&vnode->v_lock, flags);

	vfs_vnode_unref(vnode);
	return VFS_SUCCESS;
}

int
vfs_chown(const char *path, uid_t uid, gid_t gid)
{
	vnode_t *vnode;
	int ret = vfs_lookup(path, &vnode);
	if (ret != 0)
		return ret;

	uint64_t flags;
	spinlock_acquire_irqsave(&vnode->v_lock, &flags);
	vnode->v_uid = uid;
	vnode->v_gid = gid;
	vnode->v_flags |= VNODE_FLAG_DIRTY;
	spinlock_release_irqrestore(&vnode->v_lock, flags);

	vfs_vnode_unref(vnode);
	return VFS_SUCCESS;
}

int
vfs_rename(const char *oldpath, const char *newpath)
{
	if (oldpath == NULL || newpath == NULL)
		return -EINVAL;

	vnode_t *old_vnode;
	int ret = vfs_lookup(oldpath, &old_vnode);
	if (ret != 0)
		return ret;

	vnode_t *old_parent;
	char *old_name;
	ret = vfs_lookup_parent(oldpath, &old_parent, &old_name);
	if (ret != 0) {
		vfs_vnode_unref(old_vnode);
		return ret;
	}

	vnode_t *new_parent;
	char *new_name;
	ret = vfs_lookup_parent(newpath, &new_parent, &new_name);
	if (ret != 0) {
		vfs_vnode_unref(old_vnode);
		vfs_vnode_unref(old_parent);
		kfree(old_name);
		return ret;
	}

	vnode_t *target_vnode = NULL;
	ret = vfs_lookup(newpath, &target_vnode);
	if (ret == 0) {
		if ((old_vnode->v_mode & VFS_IFMT) == VFS_IFDIR &&
		    (target_vnode->v_mode & VFS_IFMT) != VFS_IFDIR) {
			vfs_vnode_unref(target_vnode);
			vfs_vnode_unref(old_vnode);
			vfs_vnode_unref(old_parent);
			vfs_vnode_unref(new_parent);
			kfree(old_name);
			kfree(new_name);
			return -ENOTDIR;
		}

		if ((old_vnode->v_mode & VFS_IFMT) != VFS_IFDIR &&
		    (target_vnode->v_mode & VFS_IFMT) == VFS_IFDIR) {
			vfs_vnode_unref(target_vnode);
			vfs_vnode_unref(old_vnode);
			vfs_vnode_unref(old_parent);
			vfs_vnode_unref(new_parent);
			kfree(old_name);
			kfree(new_name);
			return -EISDIR;
		}

		if ((target_vnode->v_mode & VFS_IFMT) == VFS_IFDIR)
			ret = new_parent->v_ops->rmdir(new_parent, new_name);
		else
			ret = new_parent->v_ops->unlink(new_parent, new_name);

		vfs_vnode_unref(target_vnode);

		if (ret != 0) {
			vfs_vnode_unref(old_vnode);
			vfs_vnode_unref(old_parent);
			vfs_vnode_unref(new_parent);
			kfree(old_name);
			kfree(new_name);
			return ret;
		}
	}

	if (new_parent->v_ops == NULL || new_parent->v_ops->link == NULL) {
		vfs_vnode_unref(old_vnode);
		vfs_vnode_unref(old_parent);
		vfs_vnode_unref(new_parent);
		kfree(old_name);
		kfree(new_name);
		return -ENOSYS;
	}

	ret = new_parent->v_ops->link(new_parent, new_name, old_vnode);
	if (ret != 0) {
		vfs_vnode_unref(old_vnode);
		vfs_vnode_unref(old_parent);
		vfs_vnode_unref(new_parent);
		kfree(old_name);
		kfree(new_name);
		return ret;
	}

	if (old_parent->v_ops == NULL || old_parent->v_ops->unlink == NULL) {
		new_parent->v_ops->unlink(new_parent, new_name);
		vfs_vnode_unref(old_vnode);
		vfs_vnode_unref(old_parent);
		vfs_vnode_unref(new_parent);
		kfree(old_name);
		kfree(new_name);
		return -ENOSYS;
	}

	ret = old_parent->v_ops->unlink(old_parent, old_name);
	if (ret != 0) {
		new_parent->v_ops->unlink(new_parent, new_name);
		vfs_vnode_unref(old_vnode);
		vfs_vnode_unref(old_parent);
		vfs_vnode_unref(new_parent);
		kfree(old_name);
		kfree(new_name);
		return ret;
	}

	vfs_dentry_remove(oldpath);
	vfs_dentry_remove(newpath);

	vfs_update_mtime(old_vnode);

	vfs_vnode_unref(old_vnode);
	vfs_vnode_unref(old_parent);
	vfs_vnode_unref(new_parent);
	kfree(old_name);
	kfree(new_name);

	VFS_DEBUG("Renamed %s to %s\n", oldpath, newpath);
	return VFS_SUCCESS;
}

int
vfs_mkdir(const char *path, mode_t mode)
{
	if (path == NULL)
		return -EINVAL;

	vnode_t *parent;
	char *name;

	int ret = vfs_lookup_parent(path, &parent, &name);
	if (ret != 0)
		return ret;

	if (parent->v_ops == NULL || parent->v_ops->mkdir == NULL) {
		kfree(name);
		vfs_vnode_unref(parent);
		return -ENOSYS;
	}

	ret = parent->v_ops->mkdir(parent, name, mode | VFS_IFDIR);
	
	if (ret == 0) {
		vfs_update_mtime(parent);
		
		uint64_t flags;
		spinlock_acquire_irqsave(&vfs_stats_lock, &flags);
		vfs_stats.creates++;
		spinlock_release_irqrestore(&vfs_stats_lock, flags);
	}

	kfree(name);
	vfs_vnode_unref(parent);

	return ret;
}

int
vfs_mknod(const char *path, mode_t mode, dev_t dev)
{
	if (path == NULL)
		return -EINVAL;

	mode_t file_type = mode & VFS_IFMT;

	switch (file_type) {
	case VFS_IFREG:
	case VFS_IFCHR:
	case VFS_IFBLK:
	case VFS_IFIFO:
	case VFS_IFSOCK:
		break;
	case VFS_IFDIR:
	case VFS_IFLNK:
		return -EPERM;
	default:
		return -EINVAL;
	}

	if ((file_type == VFS_IFCHR || file_type == VFS_IFBLK) && dev == 0) {
		VFS_DEBUG("Device files require dev parameter\n");
		return -EINVAL;
	}

	vnode_t *parent;
	char *name;

	int ret = vfs_lookup_parent(path, &parent, &name);
	if (ret != 0)
		return ret;

	if (parent->v_ops == NULL || parent->v_ops->create == NULL) {
		kfree(name);
		vfs_vnode_unref(parent);
		return -ENOSYS;
	}

	vnode_t *vnode = NULL;
	ret = parent->v_ops->create(parent, name, mode, &vnode);

	if (ret == 0 && vnode != NULL) {
		if (file_type == VFS_IFCHR || file_type == VFS_IFBLK) {
			uint64_t flags;
			spinlock_acquire_irqsave(&vnode->v_lock, &flags);
			vnode->v_rdev = dev;
			vnode->v_flags |= VNODE_FLAG_DIRTY;
			spinlock_release_irqrestore(&vnode->v_lock, flags);
		}

		vfs_vnode_unref(vnode);
		vfs_update_mtime(parent);

		VFS_DEBUG("Created node: %s (type: 0x%x, dev: 0x%x)\n",
		          path, file_type, dev);
	}

	kfree(name);
	vfs_vnode_unref(parent);

	return ret;
}

int
vfs_unlink(const char *path)
{
	if (path == NULL)
		return -EINVAL;

	vnode_t *parent;
	char *name;

	int ret = vfs_lookup_parent(path, &parent, &name);
	if (ret != 0)
		return ret;

	if (parent->v_ops == NULL || parent->v_ops->unlink == NULL) {
		kfree(name);
		vfs_vnode_unref(parent);
		return -ENOSYS;
	}

	ret = parent->v_ops->unlink(parent, name);

	if (ret == 0) {
		vfs_update_mtime(parent);
		vfs_dentry_remove(path);
		
		uint64_t flags;
		spinlock_acquire_irqsave(&vfs_stats_lock, &flags);
		vfs_stats.deletes++;
		spinlock_release_irqrestore(&vfs_stats_lock, flags);
	}

	kfree(name);
	vfs_vnode_unref(parent);

	return ret;
}

int
vfs_link(const char *oldpath, const char *newpath)
{
	if (oldpath == NULL || newpath == NULL)
		return -EINVAL;

	vnode_t *target;
	int ret = vfs_lookup(oldpath, &target);
	if (ret != 0)
		return ret;

	vnode_t *parent;
	char *name;
	ret = vfs_lookup_parent(newpath, &parent, &name);
	if (ret != 0) {
		vfs_vnode_unref(target);
		return ret;
	}

	if (parent->v_ops == NULL || parent->v_ops->link == NULL) {
		kfree(name);
		vfs_vnode_unref(parent);
		vfs_vnode_unref(target);
		return -ENOSYS;
	}

	ret = parent->v_ops->link(parent, name, target);

	if (ret == 0)
		vfs_update_mtime(parent);

	kfree(name);
	vfs_vnode_unref(parent);
	vfs_vnode_unref(target);

	return ret;
}

int
vfs_symlink(const char *target, const char *linkpath)
{
	if (target == NULL || linkpath == NULL)
		return -EINVAL;

	vnode_t *parent;
	char *name;

	int ret = vfs_lookup_parent(linkpath, &parent, &name);
	if (ret != 0)
		return ret;

	if (parent->v_ops == NULL || parent->v_ops->symlink == NULL) {
		kfree(name);
		vfs_vnode_unref(parent);
		return -ENOSYS;
	}

	ret = parent->v_ops->symlink(parent, name, target);

	if (ret == 0)
		vfs_update_mtime(parent);

	kfree(name);
	vfs_vnode_unref(parent);

	return ret;
}

int
vfs_readlink(const char *path, char *buf, size_t bufsize)
{
	if (path == NULL || buf == NULL || bufsize == 0)
		return -EINVAL;

	vnode_t *vnode;
	int ret = vfs_lookup(path, &vnode);
	if (ret != 0)
		return ret;

	if ((vnode->v_mode & VFS_IFMT) != VFS_IFLNK) {
		vfs_vnode_unref(vnode);
		return -EINVAL;
	}

	if (vnode->v_ops == NULL || vnode->v_ops->readlink == NULL) {
		vfs_vnode_unref(vnode);
		return -ENOSYS;
	}

	if (bufsize > VFS_MAX_PATH)
		bufsize = VFS_MAX_PATH;

	ret = vnode->v_ops->readlink(vnode, buf, bufsize);

	vfs_vnode_unref(vnode);
	return ret;
}

int
vfs_truncate(const char *path, off_t length)
{
	if (path == NULL)
		return -EINVAL;

	vnode_t *vnode;
	int ret = vfs_lookup(path, &vnode);
	if (ret != 0)
		return ret;

	if (vnode->v_ops == NULL || vnode->v_ops->truncate == NULL) {
		vfs_vnode_unref(vnode);
		return -ENOSYS;
	}

	ret = vnode->v_ops->truncate(vnode, length);

	if (ret == 0)
		vfs_update_mtime(vnode);

	vfs_vnode_unref(vnode);
	return ret;
}

int
vfs_sync(void)
{
	uint64_t flags;
	spinlock_acquire_irqsave(&mount_list_lock, &flags);

	vfs_mount_t *mnt = mount_list;
	while (mnt != NULL) {
		if (mnt->mnt_ops && mnt->mnt_ops->sync)
			mnt->mnt_ops->sync(mnt);
		mnt = mnt->mnt_next;
	}

	spinlock_release_irqrestore(&mount_list_lock, flags);

	return VFS_SUCCESS;
}

int
vfs_fsync(vfs_file_t *file)
{
	if (file == NULL)
		return -EINVAL;

	vnode_t *vnode = file->f_vnode;

	if (vnode->v_ops == NULL || vnode->v_ops->sync == NULL)
		return -ENOSYS;

	return vnode->v_ops->sync(vnode);
}

vfs_dentry_t *
vfs_dentry_lookup(const char *path)
{
	if (path == NULL)
		return NULL;

	uint32_t hash = dentry_hash_func(path);

	uint64_t flags;
	spinlock_acquire_irqsave(&dentry_hash_lock, &flags);

	vfs_dentry_t *dentry = dentry_hash[hash];
	while (dentry != NULL) {
		if (strcmp(dentry->d_name, path) == 0) {
			spinlock_release_irqrestore(&dentry_hash_lock, flags);
			return dentry;
		}
		dentry = dentry->d_next;
	}

	spinlock_release_irqrestore(&dentry_hash_lock, flags);
	return NULL;
}

int
vfs_dentry_add(const char *path, vnode_t *vnode)
{
	if (path == NULL || vnode == NULL)
		return -EINVAL;

	vfs_dentry_t *dentry = kmalloc(sizeof(vfs_dentry_t));
	if (dentry == NULL)
		return -ENOMEM;

	dentry->d_name = vfs_strdup(path);
	if (dentry->d_name == NULL) {
		kfree(dentry);
		return -ENOMEM;
	}

	dentry->d_vnode = vnode;
	vfs_vnode_ref(vnode);
	dentry->d_parent = NULL;
	dentry->d_hash = dentry_hash_func(path);
	spinlock_init(&dentry->d_lock, "dentry");

	uint64_t flags;
	spinlock_acquire_irqsave(&dentry_hash_lock, &flags);

	uint32_t hash = dentry->d_hash;
	dentry->d_next = dentry_hash[hash];
	dentry_hash[hash] = dentry;

	spinlock_release_irqrestore(&dentry_hash_lock, flags);

	return VFS_SUCCESS;
}

void
vfs_dentry_remove(const char *path)
{
	if (path == NULL)
		return;

	uint32_t hash = dentry_hash_func(path);

	uint64_t flags;
	spinlock_acquire_irqsave(&dentry_hash_lock, &flags);

	vfs_dentry_t **current = &dentry_hash[hash];
	while (*current != NULL) {
		if (strcmp((*current)->d_name, path) == 0) {
			vfs_dentry_t *dentry = *current;
			*current = dentry->d_next;

			spinlock_release_irqrestore(&dentry_hash_lock, flags);

			vfs_vnode_unref(dentry->d_vnode);
			kfree(dentry->d_name);
			kfree(dentry);
			return;
		}
		current = &(*current)->d_next;
	}

	spinlock_release_irqrestore(&dentry_hash_lock, flags);
}

void
vfs_dentry_purge(void)
{
	uint64_t flags;
	spinlock_acquire_irqsave(&dentry_hash_lock, &flags);

	for (int i = 0; i < DENTRY_HASH_SIZE; i++) {
		vfs_dentry_t *dentry = dentry_hash[i];
		while (dentry != NULL) {
			vfs_dentry_t *next = dentry->d_next;

			vfs_vnode_unref(dentry->d_vnode);
			kfree(dentry->d_name);
			kfree(dentry);

			dentry = next;
		}
		dentry_hash[i] = NULL;
	}

	spinlock_release_irqrestore(&dentry_hash_lock, flags);

	VFS_DEBUG("Dentry cache purged\n");
}

int
vfs_rmdir(const char *path)
{
	if (path == NULL)
		return -EINVAL;

	vnode_t *parent;
	char *name;

	int ret = vfs_lookup_parent(path, &parent, &name);
	if (ret != 0)
		return ret;

	if (parent->v_ops == NULL || parent->v_ops->rmdir == NULL) {
		kfree(name);
		vfs_vnode_unref(parent);
		return -ENOSYS;
	}

	ret = parent->v_ops->rmdir(parent, name);

	if (ret == 0) {
		vfs_update_mtime(parent);
		vfs_dentry_remove(path);
		
		uint64_t flags;
		spinlock_acquire_irqsave(&vfs_stats_lock, &flags);
		vfs_stats.deletes++;
		spinlock_release_irqrestore(&vfs_stats_lock, flags);
	}

	kfree(name);
	vfs_vnode_unref(parent);

	return ret;
}

int
vfs_readdir(vfs_file_t *file, vfs_dirent_t *dirent)
{
	if (file == NULL || dirent == NULL)
		return -EINVAL;

	vnode_t *vnode = file->f_vnode;

	if ((vnode->v_mode & VFS_IFMT) != VFS_IFDIR)
		return -ENOTDIR;

	if (vnode->v_ops == NULL || vnode->v_ops->readdir == NULL)
		return -ENOSYS;

	uint64_t flags;
	spinlock_acquire_irqsave(&file->f_lock, &flags);
	off_t offset = file->f_offset;
	spinlock_release_irqrestore(&file->f_lock, flags);

	int ret = vnode->v_ops->readdir(vnode, dirent, &offset);

	if (ret == 0) {
		spinlock_acquire_irqsave(&file->f_lock, &flags);
		file->f_offset = offset;
		spinlock_release_irqrestore(&file->f_lock, flags);
	}

	return ret;
}

int
vfs_create(const char *path, mode_t mode)
{
	if (path == NULL)
		return -EINVAL;

	vnode_t *parent;
	char *name;

	int ret = vfs_lookup_parent(path, &parent, &name);
	if (ret != 0)
		return ret;

	if (parent->v_ops == NULL || parent->v_ops->create == NULL) {
		kfree(name);
		vfs_vnode_unref(parent);
		return -ENOSYS;
	}

	vnode_t *vnode = NULL;
	ret = parent->v_ops->create(parent, name, mode | VFS_IFREG, &vnode);

	if (ret == 0 && vnode != NULL) {
		vfs_update_mtime(parent);
		vfs_vnode_unref(vnode);
		
		uint64_t flags;
		spinlock_acquire_irqsave(&vfs_stats_lock, &flags);
		vfs_stats.creates++;
		spinlock_release_irqrestore(&vfs_stats_lock, flags);
	}

	kfree(name);
	vfs_vnode_unref(parent);

	return ret;
}

vfs_context_t *
vfs_context_create(void)
{
	vfs_context_t *ctx = kmalloc(sizeof(vfs_context_t));
	if (ctx == NULL)
		return NULL;

	memset(ctx, 0, sizeof(vfs_context_t));
	
	if (root_mount && root_mount->mnt_root) {
		ctx->cwd = root_mount->mnt_root;
		ctx->root = root_mount->mnt_root;
		vfs_vnode_ref(ctx->cwd);
		vfs_vnode_ref(ctx->root);
	}
	
	ctx->umask = 0022;
	spinlock_init(&ctx->lock, "vfs_context");

	return ctx;
}

vfs_context_t *
vfs_context_dup(vfs_context_t *ctx)
{
	if (ctx == NULL)
		return NULL;

	vfs_context_t *new_ctx = kmalloc(sizeof(vfs_context_t));
	if (new_ctx == NULL)
		return NULL;

	uint64_t flags;
	spinlock_acquire_irqsave(&ctx->lock, &flags);

	new_ctx->cwd = ctx->cwd;
	new_ctx->root = ctx->root;
	new_ctx->umask = ctx->umask;

	if (new_ctx->cwd)
		vfs_vnode_ref(new_ctx->cwd);
	if (new_ctx->root)
		vfs_vnode_ref(new_ctx->root);

	spinlock_release_irqrestore(&ctx->lock, flags);

	spinlock_init(&new_ctx->lock, "vfs_context");

	return new_ctx;
}

void
vfs_context_free(vfs_context_t *ctx)
{
	if (ctx == NULL)
		return;

	if (ctx->cwd)
		vfs_vnode_unref(ctx->cwd);
	if (ctx->root)
		vfs_vnode_unref(ctx->root);

	kfree(ctx);
}

int
vfs_chdir(vfs_context_t *ctx, const char *path)
{
	if (ctx == NULL || path == NULL)
		return -EINVAL;

	vnode_t *vnode;
	int ret = vfs_lookup(path, &vnode);
	if (ret != 0)
		return ret;

	if ((vnode->v_mode & VFS_IFMT) != VFS_IFDIR) {
		vfs_vnode_unref(vnode);
		return -ENOTDIR;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&ctx->lock, &flags);

	vnode_t *old_cwd = ctx->cwd;
	ctx->cwd = vnode;

	spinlock_release_irqrestore(&ctx->lock, flags);

	if (old_cwd)
		vfs_vnode_unref(old_cwd);

	return VFS_SUCCESS;
}

int
vfs_getcwd(vfs_context_t *ctx, char *buf, size_t size)
{
	if (ctx == NULL || buf == NULL || size == 0)
		return -EINVAL;

	uint64_t flags;
	spinlock_acquire_irqsave(&ctx->lock, &flags);
	
	if (ctx->cwd == NULL) {
		spinlock_release_irqrestore(&ctx->lock, flags);
		return -ENOENT;
	}

	/* TODO: Implement proper path reconstruction */
	if (size < 2) {
		spinlock_release_irqrestore(&ctx->lock, flags);
		return -ERANGE;
	}

	buf[0] = '/';
	buf[1] = '\0';

	spinlock_release_irqrestore(&ctx->lock, flags);
	return VFS_SUCCESS;
}

bool
vfs_is_valid_filename(const char *name)
{
	if (name == NULL || name[0] == '\0')
		return false;

	size_t len = strlen(name);
	if (len >= VFS_MAX_NAME)
		return false;

	for (size_t i = 0; i < len; i++) {
		if (name[i] == '/' || name[i] == '\0')
			return false;
	}

	if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
		return false;

	return true;
}

bool
vfs_path_is_under(const char *path, const char *root)
{
	if (path == NULL || root == NULL)
		return false;

	char norm_path[VFS_MAX_PATH];
	char norm_root[VFS_MAX_PATH];

	if (vfs_normalize_path(path, norm_path) != 0)
		return false;
	if (vfs_normalize_path(root, norm_root) != 0)
		return false;

	size_t root_len = strlen(norm_root);
	
	if (strncmp(norm_path, norm_root, root_len) != 0)
		return false;

	return norm_path[root_len] == '\0' || norm_path[root_len] == '/';
}

void
vfs_get_stats(vfs_stats_t *stats)
{
	if (stats == NULL)
		return;

	uint64_t flags;
	spinlock_acquire_irqsave(&vfs_stats_lock, &flags);
	memcpy(stats, &vfs_stats, sizeof(vfs_stats_t));
	spinlock_release_irqrestore(&vfs_stats_lock, flags);
}

void
vfs_reset_stats(void)
{
	uint64_t flags;
	spinlock_acquire_irqsave(&vfs_stats_lock, &flags);
	memset(&vfs_stats, 0, sizeof(vfs_stats_t));
	spinlock_release_irqrestore(&vfs_stats_lock, flags);
}

void
vfs_print_stats(void)
{
	vfs_stats_t stats;
	vfs_get_stats(&stats);

	VFS_DEBUG("=== VFS Statistics ===\n");
	VFS_DEBUG("  Lookups:      %llu\n", stats.lookups);
	VFS_DEBUG("  Cache hits:   %llu\n", stats.cache_hits);
	VFS_DEBUG("  Cache misses: %llu\n", stats.cache_misses);
	if (stats.lookups > 0)
		VFS_DEBUG("  Hit rate:     %llu%%\n", (stats.cache_hits * 100) / stats.lookups);
	VFS_DEBUG("  Reads:        %llu\n", stats.reads);
	VFS_DEBUG("  Writes:       %llu\n", stats.writes);
	VFS_DEBUG("  Creates:      %llu\n", stats.creates);
	VFS_DEBUG("  Deletes:      %llu\n", stats.deletes);
	VFS_DEBUG("======================\n");
}

void
vfs_dump_mounts(void)
{
	VFS_DEBUG("=== Mounted Filesystems ===\n");

	uint64_t flags;
	spinlock_acquire_irqsave(&mount_list_lock, &flags);

	vfs_mount_t *mnt = mount_list;
	int count = 0;

	while (mnt != NULL) {
		VFS_DEBUG("  [%d] %s on %s (type: %s, flags: 0x%x)\n",
		          count++,
		          mnt->mnt_device ? mnt->mnt_device : "(none)",
		          mnt->mnt_point,
		          mnt->mnt_ops ? mnt->mnt_ops->fs_name : "unknown",
		          mnt->mnt_flags);
		mnt = mnt->mnt_next;
	}

	if (count == 0)
		VFS_DEBUG("  No filesystems mounted\n");

	spinlock_release_irqrestore(&mount_list_lock, flags);
	VFS_DEBUG("===========================\n");
}

void
vfs_dump_vnodes(void)
{
	VFS_DEBUG("=== VNode Cache ===\n");

	int total = 0;
	for (int i = 0; i < VNODE_HASH_SIZE; i++) {
		uint64_t flags;
		spinlock_acquire_irqsave(&vnode_hash_locks[i], &flags);

		vnode_t *vnode = vnode_hash[i];
		int bucket_count = 0;

		while (vnode != NULL) {
			bucket_count++;
			total++;
			vnode = vnode->v_next;
		}

		spinlock_release_irqrestore(&vnode_hash_locks[i], flags);

		if (bucket_count > 0)
			VFS_DEBUG("  Bucket %d: %d vnodes\n", i, bucket_count);
	}

	VFS_DEBUG("  Total vnodes: %d\n", total);
	VFS_DEBUG("===================\n");
}
