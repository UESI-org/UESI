/* Complex Software, keep heavily commented!!! */

#include <tmpfs.h>
#include <errno.h>
#include <sys/panic.h>
#include <sys/stat.h>
#include <kmalloc.h>
#include <kfree.h>
#include <string.h>

extern void serial_printf(uint16_t port, const char *fmt, ...);
#define DEBUG_PORT 0x3F8
#define TMPFS_DEBUG(fmt, ...)                                                  \
	serial_printf(DEBUG_PORT, "[TMPFS] " fmt, ##__VA_ARGS__)

static vnode_ops_t tmpfs_vnode_ops;
static vfs_ops_t tmpfs_vfs_ops;

int
tmpfs_init(void)
{
	TMPFS_DEBUG("Initializing tmpfs\n");

	/* Set up VFS operations */
	tmpfs_vfs_ops.fs_name = "tmpfs";
	tmpfs_vfs_ops.mount = tmpfs_mount;
	tmpfs_vfs_ops.unmount = tmpfs_unmount;
	tmpfs_vfs_ops.statfs = tmpfs_statfs;
	tmpfs_vfs_ops.sync = tmpfs_sync;
	tmpfs_vfs_ops.alloc_vnode = NULL; /* We handle this internally */
	tmpfs_vfs_ops.free_vnode = NULL;

	/* Register with VFS */
	int ret = vfs_register_filesystem(&tmpfs_vfs_ops);
	if (ret != 0) {
		TMPFS_DEBUG("Failed to register tmpfs: %d\n", ret);
		return ret;
	}

	TMPFS_DEBUG("tmpfs initialized successfully\n");
	return 0;
}

tmpfs_node_t *
tmpfs_node_alloc(tmpfs_mount_t *tm, uint8_t type, mode_t mode)
{
	if (tm == NULL) {
		return NULL;
	}

	tmpfs_node_t *node = kmalloc(sizeof(tmpfs_node_t));
	if (node == NULL) {
		return NULL;
	}

	memset(node, 0, sizeof(tmpfs_node_t));

	/* Acquire mount lock to get next inode number */
	uint64_t flags;
	spinlock_acquire_irqsave(&tm->tm_lock, &flags);
	node->tn_ino = tm->tm_next_ino++;
	spinlock_release_irqrestore(&tm->tm_lock, flags);

	node->tn_type = type;
	node->tn_mode = mode;
	node->tn_uid = 0; /* TODO: Get from current process */
	node->tn_gid = 0;
	node->tn_nlink = 1;
	node->tn_size = 0;

	/* Get current time - for now use a simple counter */
	/* TODO: Get real time from system clock */
	node->tn_atime = node->tn_mtime = node->tn_ctime = 0;

	node->tn_mount = tm->tm_vfs_mount; /* Store VFS mount pointer */
	spinlock_init(&node->tn_lock, "tmpfs_node");

	/* Initialize type-specific data */
	switch (type) {
	case TMPFS_FILE:
		node->tn_data.file.data = NULL;
		node->tn_data.file.capacity = 0;
		break;
	case TMPFS_DIR:
		node->tn_data.dir.entries = NULL;
		node->tn_data.dir.count = 0;
		break;
	case TMPFS_SYMLINK:
		node->tn_data.symlink.target = NULL;
		break;
	}

	return node;
}

void
tmpfs_node_free(tmpfs_node_t *node)
{
	if (node == NULL) {
		return;
	}

	/* Free type-specific data */
	switch (node->tn_type) {
	case TMPFS_FILE:
		if (node->tn_data.file.data != NULL) {
			kfree(node->tn_data.file.data);
		}
		break;
	case TMPFS_DIR:
		/* Free all directory entries */
		tmpfs_dirent_t *entry = node->tn_data.dir.entries;
		while (entry != NULL) {
			tmpfs_dirent_t *next = entry->td_next;
			if (entry->td_name != NULL) {
				kfree(entry->td_name);
			}
			kfree(entry);
			entry = next;
		}
		break;
	case TMPFS_SYMLINK:
		if (node->tn_data.symlink.target != NULL) {
			kfree(node->tn_data.symlink.target);
		}
		break;
	}

	kfree(node);
}

tmpfs_node_t *
tmpfs_vnode_to_node(vnode_t *vnode)
{
	if (vnode == NULL) {
		return NULL;
	}
	return (tmpfs_node_t *)vnode->v_private;
}

int
tmpfs_node_to_vnode(tmpfs_node_t *node, vnode_t **vnode)
{
	if (node == NULL || vnode == NULL) {
		return -EINVAL;
	}

	/* Allocate vnode */
	vnode_t *v = vfs_vnode_alloc(node->tn_mount, node->tn_ino);
	if (v == NULL) {
		return -ENOMEM;
	}

	/* Set vnode attributes based on node type */
	mode_t vfs_type;
	switch (node->tn_type) {
	case TMPFS_FILE:
		vfs_type = VFS_IFREG;
		break;
	case TMPFS_DIR:
		vfs_type = VFS_IFDIR;
		break;
	case TMPFS_SYMLINK:
		vfs_type = VFS_IFLNK;
		break;
	default:
		vfs_vnode_free(v);
		return -EINVAL;
	}

	v->v_mode = (node->tn_mode & ~VFS_IFMT) | vfs_type;
	v->v_uid = node->tn_uid;
	v->v_gid = node->tn_gid;
	v->v_size = node->tn_size;
	v->v_nlink = node->tn_nlink;
	v->v_atime = node->tn_atime;
	v->v_mtime = node->tn_mtime;
	v->v_ctime = node->tn_ctime;
	v->v_private = node;
	v->v_ops = &tmpfs_vnode_ops;

	*vnode = v;
	return 0;
}

int
tmpfs_mount(const char *device,
            const char *mountpoint,
            uint32_t flags,
            void *data,
            vfs_mount_t **mnt)
{
	(void)device; /* Unused - tmpfs doesn't need a device */
	(void)data;   /* Unused - could be used for mount options later */

	TMPFS_DEBUG("Mounting tmpfs at %s\n", mountpoint);

	/* Allocate mount structure */
	vfs_mount_t *mount = kmalloc(sizeof(vfs_mount_t));
	if (mount == NULL) {
		return -ENOMEM;
	}
	memset(mount, 0, sizeof(vfs_mount_t));

	/* Allocate tmpfs-specific data */
	tmpfs_mount_t *tm = kmalloc(sizeof(tmpfs_mount_t));
	if (tm == NULL) {
		kfree(mount);
		return -ENOMEM;
	}
	memset(tm, 0, sizeof(tmpfs_mount_t));

	/* Initialize tmpfs mount data */
	tm->tm_max_size = 64 * 1024 * 1024; /* 64MB default */
	tm->tm_used_size = 0;
	tm->tm_next_ino = TMPFS_ROOT_INO;
	tm->tm_vfs_mount = mount; /* Store back pointer */
	spinlock_init(&tm->tm_lock, "tmpfs_mount");

	/* Create root directory node */
	tmpfs_node_t *root =
	    tmpfs_node_alloc(tm, TMPFS_DIR, S_IFDIR | TMPFS_DEFAULT_MODE);
	if (root == NULL) {
		kfree(tm);
		kfree(mount);
		return -ENOMEM;
	}
	tm->tm_root = root;

	/* Create root vnode */
	vnode_t *root_vnode;
	int ret = tmpfs_node_to_vnode(root, &root_vnode);
	if (ret != 0) {
		tmpfs_node_free(root);
		kfree(tm);
		kfree(mount);
		return ret;
	}
	root_vnode->v_flags |= VNODE_FLAG_ROOT;

	/* Set up mount structure */
	mount->mnt_point = kmalloc(strlen(mountpoint) + 1);
	if (mount->mnt_point == NULL) {
		vfs_vnode_free(root_vnode);
		tmpfs_node_free(root);
		kfree(tm);
		kfree(mount);
		return -ENOMEM;
	}
	strcpy(mount->mnt_point, mountpoint);

	mount->mnt_device = NULL;
	mount->mnt_flags = flags;
	mount->mnt_root = root_vnode;
	mount->mnt_ops = &tmpfs_vfs_ops;
	mount->mnt_private = tm;
	spinlock_init(&mount->mnt_lock, "tmpfs_vfs_mount");

	*mnt = mount;

	TMPFS_DEBUG("tmpfs mounted successfully at %s\n", mountpoint);
	return 0;
}

int
tmpfs_unmount(vfs_mount_t *mnt, uint32_t flags)
{
	(void)flags; /* Unused - could be used for force unmount later */

	if (mnt == NULL) {
		return -EINVAL;
	}

	tmpfs_mount_t *tm = (tmpfs_mount_t *)mnt->mnt_private;
	if (tm == NULL) {
		return -EINVAL;
	}

	TMPFS_DEBUG("Unmounting tmpfs from %s\n", mnt->mnt_point);

	/* TODO: Walk entire directory tree and free all nodes */
	/* For now, just free the root */
	if (tm->tm_root != NULL) {
		tmpfs_node_free(tm->tm_root);
	}

	kfree(tm);

	return 0;
}

int
tmpfs_statfs(vfs_mount_t *mnt, void *statbuf)
{
	(void)mnt;     /* Unused for now */
	(void)statbuf; /* Unused for now */
	/* TODO: Implement if needed */
	return -ENOSYS;
}

int
tmpfs_sync(vfs_mount_t *mnt)
{
	(void)mnt; /* Unused - nothing to sync for in-memory filesystem */
	/* Nothing to sync for in-memory filesystem */
	return 0;
}

ssize_t
tmpfs_read(vnode_t *vnode, void *buf, size_t count, off_t offset)
{
	if (vnode == NULL || buf == NULL) {
		return -EINVAL;
	}

	tmpfs_node_t *node = tmpfs_vnode_to_node(vnode);
	if (node == NULL || node->tn_type != TMPFS_FILE) {
		return -EINVAL;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&node->tn_lock, &flags);

	/* Check if offset is beyond file size */
	if (offset >= node->tn_size) {
		spinlock_release_irqrestore(&node->tn_lock, flags);
		return 0;
	}

	/* Calculate how much we can actually read */
	size_t available = node->tn_size - offset;
	size_t to_read = (count < available) ? count : available;

	/* Copy data */
	if (node->tn_data.file.data != NULL && to_read > 0) {
		memcpy(buf, (char *)node->tn_data.file.data + offset, to_read);
	}

	/* Update access time */
	/* node->tn_atime = current_time(); */

	spinlock_release_irqrestore(&node->tn_lock, flags);

	return to_read;
}

ssize_t
tmpfs_write(vnode_t *vnode, const void *buf, size_t count, off_t offset)
{
	if (vnode == NULL || buf == NULL) {
		return -EINVAL;
	}

	tmpfs_node_t *node = tmpfs_vnode_to_node(vnode);
	if (node == NULL || node->tn_type != TMPFS_FILE) {
		return -EINVAL;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&node->tn_lock, &flags);

	/* Calculate new size needed */
	size_t new_size = offset + count;

	/* Reallocate buffer if needed */
	if (new_size > node->tn_data.file.capacity) {
		size_t new_capacity = new_size * 2; /* Grow by 2x */
		void *new_data = kmalloc(new_capacity);
		if (new_data == NULL) {
			spinlock_release_irqrestore(&node->tn_lock, flags);
			return -ENOMEM;
		}

		/* Copy old data if any */
		if (node->tn_data.file.data != NULL) {
			memcpy(
			    new_data, node->tn_data.file.data, node->tn_size);
			kfree(node->tn_data.file.data);
		}

		node->tn_data.file.data = new_data;
		node->tn_data.file.capacity = new_capacity;
	}

	/* Write data */
	memcpy((char *)node->tn_data.file.data + offset, buf, count);

	/* Update size if needed */
	if ((off_t)new_size > node->tn_size) {
		node->tn_size = new_size;
		vnode->v_size = new_size;
	}

	/* Update modification time */
	/* node->tn_mtime = node->tn_ctime = current_time(); */

	spinlock_release_irqrestore(&node->tn_lock, flags);

	return count;
}

int
tmpfs_truncate(vnode_t *vnode, off_t length)
{
	if (vnode == NULL || length < 0) {
		return -EINVAL;
	}

	tmpfs_node_t *node = tmpfs_vnode_to_node(vnode);
	if (node == NULL || node->tn_type != TMPFS_FILE) {
		return -EINVAL;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&node->tn_lock, &flags);

	if (length == 0) {
		/* Free all data */
		if (node->tn_data.file.data != NULL) {
			kfree(node->tn_data.file.data);
			node->tn_data.file.data = NULL;
		}
		node->tn_data.file.capacity = 0;
		node->tn_size = 0;
	} else if ((size_t)length < node->tn_size) {
		/* Shrink file */
		node->tn_size = length;
	} else if ((size_t)length > node->tn_size) {
		/* Grow file - allocate more space */
		if ((size_t)length > node->tn_data.file.capacity) {
			void *new_data = kmalloc(length);
			if (new_data == NULL) {
				spinlock_release_irqrestore(&node->tn_lock,
				                            flags);
				return -ENOMEM;
			}

			/* Copy old data and zero new space */
			if (node->tn_data.file.data != NULL) {
				memcpy(new_data,
				       node->tn_data.file.data,
				       node->tn_size);
				kfree(node->tn_data.file.data);
			}
			memset((char *)new_data + node->tn_size,
			       0,
			       length - node->tn_size);

			node->tn_data.file.data = new_data;
			node->tn_data.file.capacity = length;
		}
		node->tn_size = length;
	}

	vnode->v_size = node->tn_size;

	spinlock_release_irqrestore(&node->tn_lock, flags);

	return 0;
}

tmpfs_dirent_t *
tmpfs_dirent_find(tmpfs_node_t *dir, const char *name)
{
	if (dir == NULL || name == NULL || dir->tn_type != TMPFS_DIR) {
		return NULL;
	}

	tmpfs_dirent_t *entry = dir->tn_data.dir.entries;
	while (entry != NULL) {
		if (strcmp(entry->td_name, name) == 0) {
			return entry;
		}
		entry = entry->td_next;
	}

	return NULL;
}

int
tmpfs_dirent_add(tmpfs_node_t *dir, const char *name, tmpfs_node_t *node)
{
	if (dir == NULL || name == NULL || node == NULL ||
	    dir->tn_type != TMPFS_DIR) {
		return -EINVAL;
	}

	/* Check if entry already exists */
	if (tmpfs_dirent_find(dir, name) != NULL) {
		return -EEXIST;
	}

	/* Allocate new entry */
	tmpfs_dirent_t *entry = kmalloc(sizeof(tmpfs_dirent_t));
	if (entry == NULL) {
		return -ENOMEM;
	}

	entry->td_name = kmalloc(strlen(name) + 1);
	if (entry->td_name == NULL) {
		kfree(entry);
		return -ENOMEM;
	}
	strcpy(entry->td_name, name);

	entry->td_node = node;
	entry->td_next = dir->tn_data.dir.entries;
	dir->tn_data.dir.entries = entry;
	dir->tn_data.dir.count++;

	/* Increment link count on node */
	node->tn_nlink++;

	return 0;
}

int
tmpfs_dirent_remove(tmpfs_node_t *dir, const char *name)
{
	if (dir == NULL || name == NULL || dir->tn_type != TMPFS_DIR) {
		return -EINVAL;
	}

	tmpfs_dirent_t **prev = &dir->tn_data.dir.entries;
	tmpfs_dirent_t *entry = *prev;

	while (entry != NULL) {
		if (strcmp(entry->td_name, name) == 0) {
			*prev = entry->td_next;
			dir->tn_data.dir.count--;

			/* Decrement link count */
			entry->td_node->tn_nlink--;

			/* Free entry if link count reaches zero */
			if (entry->td_node->tn_nlink == 0) {
				tmpfs_node_free(entry->td_node);
			}

			kfree(entry->td_name);
			kfree(entry);

			return 0;
		}
		prev = &entry->td_next;
		entry = entry->td_next;
	}

	return -ENOENT;
}

int
tmpfs_readdir(vnode_t *vnode, vfs_dirent_t *dirent, off_t *offset)
{
	if (vnode == NULL || dirent == NULL || offset == NULL) {
		return -EINVAL;
	}

	tmpfs_node_t *node = tmpfs_vnode_to_node(vnode);
	if (node == NULL || node->tn_type != TMPFS_DIR) {
		return -ENOTDIR;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&node->tn_lock, &flags);

	/* Navigate to the entry at the given offset */
	tmpfs_dirent_t *entry = node->tn_data.dir.entries;
	off_t current = 0;

	while (entry != NULL && current < *offset) {
		entry = entry->td_next;
		current++;
	}

	if (entry == NULL) {
		spinlock_release_irqrestore(&node->tn_lock, flags);
		return -ENOENT; /* End of directory */
	}

	/* Fill in dirent */
	dirent->d_ino = entry->td_node->tn_ino;
	dirent->d_off = current + 1;

	size_t name_len = strlen(entry->td_name);
	if (name_len >= VFS_MAX_NAME) {
		name_len = VFS_MAX_NAME - 1;
	}
	memcpy(dirent->d_name, entry->td_name, name_len);
	dirent->d_name[name_len] = '\0';

	/* Set file type */
	switch (entry->td_node->tn_type) {
	case TMPFS_FILE:
		dirent->d_type = VFS_IFREG >> 12;
		break;
	case TMPFS_DIR:
		dirent->d_type = VFS_IFDIR >> 12;
		break;
	case TMPFS_SYMLINK:
		dirent->d_type = VFS_IFLNK >> 12;
		break;
	default:
		dirent->d_type = 0;
	}

	dirent->d_reclen = sizeof(vfs_dirent_t);

	*offset = current + 1;

	spinlock_release_irqrestore(&node->tn_lock, flags);

	return 0;
}

int
tmpfs_lookup(vnode_t *dir, const char *name, vnode_t **result)
{
	if (dir == NULL || name == NULL || result == NULL) {
		return -EINVAL;
	}

	tmpfs_node_t *dir_node = tmpfs_vnode_to_node(dir);
	if (dir_node == NULL || dir_node->tn_type != TMPFS_DIR) {
		return -ENOTDIR;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&dir_node->tn_lock, &flags);

	tmpfs_dirent_t *entry = tmpfs_dirent_find(dir_node, name);
	if (entry == NULL) {
		spinlock_release_irqrestore(&dir_node->tn_lock, flags);
		return -ENOENT;
	}

	tmpfs_node_t *node = entry->td_node;
	spinlock_release_irqrestore(&dir_node->tn_lock, flags);

	/* Convert to vnode */
	return tmpfs_node_to_vnode(node, result);
}

int
tmpfs_create(vnode_t *dir, const char *name, mode_t mode, vnode_t **result)
{
	if (dir == NULL || name == NULL) {
		return -EINVAL;
	}

	tmpfs_node_t *dir_node = tmpfs_vnode_to_node(dir);
	if (dir_node == NULL || dir_node->tn_type != TMPFS_DIR) {
		return -ENOTDIR;
	}

	/* Get tmpfs mount structure from VFS mount */
	tmpfs_mount_t *tm = (tmpfs_mount_t *)dir_node->tn_mount->mnt_private;

	/* Create new file node */
	tmpfs_node_t *file = tmpfs_node_alloc(tm, TMPFS_FILE, mode);
	if (file == NULL) {
		return -ENOMEM;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&dir_node->tn_lock, &flags);

	/* Add to directory */
	int ret = tmpfs_dirent_add(dir_node, name, file);
	if (ret != 0) {
		spinlock_release_irqrestore(&dir_node->tn_lock, flags);
		tmpfs_node_free(file);
		return ret;
	}

	spinlock_release_irqrestore(&dir_node->tn_lock, flags);

	/* Convert to vnode if requested */
	if (result != NULL) {
		ret = tmpfs_node_to_vnode(file, result);
		if (ret != 0) {
			return ret;
		}
	}

	return 0;
}

int
tmpfs_mkdir(vnode_t *dir, const char *name, mode_t mode)
{
	if (dir == NULL || name == NULL) {
		return -EINVAL;
	}

	tmpfs_node_t *dir_node = tmpfs_vnode_to_node(dir);
	if (dir_node == NULL || dir_node->tn_type != TMPFS_DIR) {
		return -ENOTDIR;
	}

	/* Get tmpfs mount structure from VFS mount */
	tmpfs_mount_t *tm = (tmpfs_mount_t *)dir_node->tn_mount->mnt_private;

	/* Create new directory node */
	tmpfs_node_t *new_dir = tmpfs_node_alloc(tm, TMPFS_DIR, mode);
	if (new_dir == NULL) {
		return -ENOMEM;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&dir_node->tn_lock, &flags);

	/* Add to parent directory */
	int ret = tmpfs_dirent_add(dir_node, name, new_dir);
	if (ret != 0) {
		spinlock_release_irqrestore(&dir_node->tn_lock, flags);
		tmpfs_node_free(new_dir);
		return ret;
	}

	spinlock_release_irqrestore(&dir_node->tn_lock, flags);

	return 0;
}

int
tmpfs_rmdir(vnode_t *dir, const char *name)
{
	if (dir == NULL || name == NULL) {
		return -EINVAL;
	}

	tmpfs_node_t *dir_node = tmpfs_vnode_to_node(dir);
	if (dir_node == NULL || dir_node->tn_type != TMPFS_DIR) {
		return -ENOTDIR;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&dir_node->tn_lock, &flags);

	/* Find the entry */
	tmpfs_dirent_t *entry = tmpfs_dirent_find(dir_node, name);
	if (entry == NULL) {
		spinlock_release_irqrestore(&dir_node->tn_lock, flags);
		return -ENOENT;
	}

	/* Check if it's a directory */
	if (entry->td_node->tn_type != TMPFS_DIR) {
		spinlock_release_irqrestore(&dir_node->tn_lock, flags);
		return -ENOTDIR;
	}

	/* Check if directory is empty */
	if (entry->td_node->tn_data.dir.count > 0) {
		spinlock_release_irqrestore(&dir_node->tn_lock, flags);
		return -ENOTEMPTY;
	}

	/* Remove the entry */
	int ret = tmpfs_dirent_remove(dir_node, name);

	spinlock_release_irqrestore(&dir_node->tn_lock, flags);

	return ret;
}

int
tmpfs_unlink(vnode_t *dir, const char *name)
{
	if (dir == NULL || name == NULL) {
		return -EINVAL;
	}

	tmpfs_node_t *dir_node = tmpfs_vnode_to_node(dir);
	if (dir_node == NULL || dir_node->tn_type != TMPFS_DIR) {
		return -ENOTDIR;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&dir_node->tn_lock, &flags);

	int ret = tmpfs_dirent_remove(dir_node, name);

	spinlock_release_irqrestore(&dir_node->tn_lock, flags);

	return ret;
}

int
tmpfs_link(vnode_t *dir, const char *name, vnode_t *target)
{
	if (dir == NULL || name == NULL || target == NULL) {
		return -EINVAL;
	}

	tmpfs_node_t *dir_node = tmpfs_vnode_to_node(dir);
	tmpfs_node_t *target_node = tmpfs_vnode_to_node(target);

	if (dir_node == NULL || target_node == NULL ||
	    dir_node->tn_type != TMPFS_DIR) {
		return -EINVAL;
	}

	/* Can't link directories */
	if (target_node->tn_type == TMPFS_DIR) {
		return -EPERM;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&dir_node->tn_lock, &flags);

	int ret = tmpfs_dirent_add(dir_node, name, target_node);

	spinlock_release_irqrestore(&dir_node->tn_lock, flags);

	return ret;
}

int
tmpfs_symlink(vnode_t *dir, const char *name, const char *target)
{
	if (dir == NULL || name == NULL || target == NULL) {
		return -EINVAL;
	}

	tmpfs_node_t *dir_node = tmpfs_vnode_to_node(dir);
	if (dir_node == NULL || dir_node->tn_type != TMPFS_DIR) {
		return -ENOTDIR;
	}

	/* Get tmpfs mount structure from VFS mount */
	tmpfs_mount_t *tm = (tmpfs_mount_t *)dir_node->tn_mount->mnt_private;

	/* Create new symlink node */
	tmpfs_node_t *symlink =
	    tmpfs_node_alloc(tm, TMPFS_SYMLINK, S_IFLNK | 0777);
	if (symlink == NULL) {
		return -ENOMEM;
	}

	/* Store target path */
	symlink->tn_data.symlink.target = kmalloc(strlen(target) + 1);
	if (symlink->tn_data.symlink.target == NULL) {
		tmpfs_node_free(symlink);
		return -ENOMEM;
	}
	strcpy(symlink->tn_data.symlink.target, target);
	symlink->tn_size = strlen(target);

	uint64_t flags;
	spinlock_acquire_irqsave(&dir_node->tn_lock, &flags);

	/* Add to directory */
	int ret = tmpfs_dirent_add(dir_node, name, symlink);
	if (ret != 0) {
		spinlock_release_irqrestore(&dir_node->tn_lock, flags);
		tmpfs_node_free(symlink);
		return ret;
	}

	spinlock_release_irqrestore(&dir_node->tn_lock, flags);

	return 0;
}

int
tmpfs_readlink(vnode_t *vnode, char *buf, size_t bufsize)
{
	if (vnode == NULL || buf == NULL || bufsize == 0) {
		return -EINVAL;
	}

	tmpfs_node_t *node = tmpfs_vnode_to_node(vnode);
	if (node == NULL || node->tn_type != TMPFS_SYMLINK) {
		return -EINVAL;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&node->tn_lock, &flags);

	if (node->tn_data.symlink.target == NULL) {
		spinlock_release_irqrestore(&node->tn_lock, flags);
		return -EINVAL;
	}

	size_t len = strlen(node->tn_data.symlink.target);
	if (len >= bufsize) {
		len = bufsize - 1;
	}

	memcpy(buf, node->tn_data.symlink.target, len);
	buf[len] = '\0';

	spinlock_release_irqrestore(&node->tn_lock, flags);

	return len;
}

int
tmpfs_getattr(vnode_t *vnode, vfs_stat_t *stat)
{
	if (vnode == NULL || stat == NULL) {
		return -EINVAL;
	}

	tmpfs_node_t *node = tmpfs_vnode_to_node(vnode);
	if (node == NULL) {
		return -EINVAL;
	}

	memset(stat, 0, sizeof(vfs_stat_t));

	stat->st_dev = 0; /* TODO: Proper device ID */
	stat->st_ino = node->tn_ino;
	stat->st_mode = vnode->v_mode;
	stat->st_nlink = node->tn_nlink;
	stat->st_uid = node->tn_uid;
	stat->st_gid = node->tn_gid;
	stat->st_rdev = 0;
	stat->st_size = node->tn_size;
	stat->st_blksize = 512;
	stat->st_blocks = (node->tn_size + 511) / 512;
	stat->st_atime = node->tn_atime;
	stat->st_mtime = node->tn_mtime;
	stat->st_ctime = node->tn_ctime;

	return 0;
}

int
tmpfs_setattr(vnode_t *vnode, vfs_stat_t *stat)
{
	if (vnode == NULL || stat == NULL) {
		return -EINVAL;
	}

	tmpfs_node_t *node = tmpfs_vnode_to_node(vnode);
	if (node == NULL) {
		return -EINVAL;
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&node->tn_lock, &flags);

	/* Update attributes */
	node->tn_mode = stat->st_mode;
	node->tn_uid = stat->st_uid;
	node->tn_gid = stat->st_gid;

	vnode->v_mode = stat->st_mode;
	vnode->v_uid = stat->st_uid;
	vnode->v_gid = stat->st_gid;

	spinlock_release_irqrestore(&node->tn_lock, flags);

	return 0;
}

void
tmpfs_release(vnode_t *vnode)
{
	(void)vnode; /* Unused - node is freed when unlinked */
	/* Nothing special to do - node is freed when unlinked */
	return;
}

static vnode_ops_t tmpfs_vnode_ops = { .read = tmpfs_read,
	                               .write = tmpfs_write,
	                               .truncate = tmpfs_truncate,
	                               .readdir = tmpfs_readdir,
	                               .lookup = tmpfs_lookup,
	                               .create = tmpfs_create,
	                               .mkdir = tmpfs_mkdir,
	                               .rmdir = tmpfs_rmdir,
	                               .unlink = tmpfs_unlink,
	                               .link = tmpfs_link,
	                               .symlink = tmpfs_symlink,
	                               .readlink = tmpfs_readlink,
	                               .getattr = tmpfs_getattr,
	                               .setattr = tmpfs_setattr,
	                               .sync =
	                                   NULL, /* No sync needed for tmpfs */
	                               .release = tmpfs_release };