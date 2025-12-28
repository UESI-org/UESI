#include <stdbool.h>
#include <syscall.h>
#include <scheduler.h>
#include <vfs.h>
#include <idt.h>
#include <fcntl.h>
#include <gdt.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/mman.h>
#include <vmm.h>
#include <pmm.h>
#include <proc.h>
#include <string.h>
#include <tapframe.h>
#include <kmalloc.h>
#include <kfree.h>

extern bool keyboard_has_key(void);
extern char keyboard_getchar(void);
extern void tty_printf(const char *fmt, ...);
extern void tty_putchar(char c);
extern void syscall_stub(void);

extern int kern_sysinfo(struct sysinfo *info);
extern int gethostname(char *name, size_t len);
extern int gethostid(void);

extern int errno;

#define USER_SPACE_START 0x0000000000400000UL
#define USER_SPACE_END 0x00007FFFFFFFFFFFUL

static inline bool
is_user_address(const void *addr)
{
	uint64_t ptr = (uint64_t)addr;
	return ptr >= USER_SPACE_START && ptr < USER_SPACE_END;
}

static inline bool
is_user_range(const void *addr, size_t len)
{
	if (len == 0)
		return true;
	uint64_t start = (uint64_t)addr;
	uint64_t end = start + len;

	if (end < start)
		return false;

	return is_user_address(addr) && end <= USER_SPACE_END;
}

static task_t *
scheduler_create_forked_task(struct proc *p)
{
    if (!p || !p->p_p) {
        return NULL;
    }
	
    return p;
}

static uint64_t
mmap_prot_to_page_flags(int prot, bool is_user)
{
	uint64_t flags = PAGE_PRESENT;

	if (prot & PROT_WRITE) {
		flags |= PAGE_WRITE;
	}

	if (!(prot & PROT_EXEC)) {
		flags |= PAGE_NX;
	}

	if (is_user) {
		flags |= PAGE_USER;
	}

	return flags;
}

static uint64_t
find_free_region(vmm_address_space_t *space, size_t size)
{
	if (!space) {
		return 0;
	}

	uint64_t search_start = VMM_USER_HEAP_START;
	uint64_t search_end = VMM_USER_STACK_TOP - VMM_USER_STACK_SIZE;

	size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

	uint64_t current = search_start;

	while (current + size <= search_end) {
		bool region_free = true;

		vmm_region_t *region = space->regions;
		while (region != NULL) {
			uint64_t region_end = current + size;

			if (!(region_end <= region->virt_start ||
			      current >= region->virt_end)) {
				region_free = false;
				current = (region->virt_end + PAGE_SIZE - 1) &
				          ~(PAGE_SIZE - 1);
				break;
			}
			region = region->next;
		}

		if (region_free) {
			return current;
		}
	}

	return 0;
}

static inline int
vfs_errno(int vfs_ret)
{
	if (vfs_ret == VFS_SUCCESS)
		return 0;
	if (vfs_ret < 0)
		return -vfs_ret;
	return EIO;
}

void
sys_exit(int status)
{
	task_t *current = scheduler_get_current_task();

	if (current) {
		struct proc *p = proc_get_current();

		if (p != NULL && p->p_p != NULL && p->p_p->ps_vmspace != NULL) {
			struct process *ps = p->p_p;

			tty_printf("[SYSCALL] Cleaning up memory mappings for "
			           "task %d\n",
			           current->p_tid);

			extern struct limine_hhdm_response *boot_get_hhdm(void);
			struct limine_hhdm_response *hhdm = boot_get_hhdm();
			uint64_t hhdm_offset = hhdm ? hhdm->offset : 0;

			uint64_t start = USER_SPACE_START;
			uint64_t end = ps->ps_brk;

			if (end > start) {
				uint64_t current_page =
				    (start + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

				while (current_page < end) {
					uint64_t phys_addr =
					    mmu_get_physical_address(
					        ps->ps_vmspace, current_page);

					if (phys_addr != 0) {
						paging_unmap_range(
						    ps->ps_vmspace,
						    current_page,
						    1);

						void *page_virt =
						    (void *)(phys_addr +
						             hhdm_offset);
						pmm_free(page_virt);
					}

					current_page += PAGE_SIZE;
				}

				tty_printf("[SYSCALL] Unmapped memory range "
				           "0x%lx - 0x%lx\n",
				           start,
				           end);
			}
		}

		for (int i = 0; i < MAX_OPEN_FILES; i++) {
			if (current->p_p->ps_fd_table[i].file != NULL) {
				vfs_file_t *file =
				    (vfs_file_t *)current->p_p->ps_fd_table[i].file;
				vfs_close(file);
				current->p_p->ps_fd_table[i].file = NULL;
				current->p_p->ps_fd_table[i].flags = 0;
			}
		}

		tty_printf("[SYSCALL] Task %d (%s) exiting with status %d\n",
		           current->p_tid,
		           current->p_name,
		           status);

		scheduler_exit_task(status);

		while (1) {
			asm volatile("hlt");
		}
	} else {
		tty_printf(
		    "\nProcess exited with status: %d (no scheduler context)\n",
		    status);
		asm volatile("cli; hlt");
		while (1)
			;
	}
}

int64_t
sys_read(int fd, void *buf, size_t count)
{
	if (count == 0) {
		return 0;
	}

	if (!is_user_range(buf, count)) {
		return -EFAULT;
	}

	if (fd == 0) {
		char *buffer = (char *)buf;
		size_t bytes_read = 0;
		bool got_newline = false;

		while (bytes_read < count && !got_newline) {
			asm volatile("sti");

			while (!keyboard_has_key()) {
				asm volatile("sti; hlt");
			}

			char c = keyboard_getchar();

			if (c == '\n' || c == '\r') {
				buffer[bytes_read++] = '\n';
				tty_putchar('\n'); // Echo newline
				got_newline = true;
				break;
			}

			if (c == '\b' || c == 0x08 || c == 0x7F) {
				if (bytes_read > 0) {
					bytes_read--;
					tty_putchar('\b');
					tty_putchar(' ');
					tty_putchar('\b');
				}
				continue;
			}

			if (c == 3) {
				tty_putchar('^');
				tty_putchar('C');
				tty_putchar('\n');
				return -EINTR;
			}

			if (c == 4) {
				if (bytes_read == 0) {
					return 0; /* EOF on empty read */
				}
				got_newline = true;
				break;
			}

			if (bytes_read < count) {
				buffer[bytes_read++] = c;
				tty_putchar(c); // Echo the character since
				                // callback is removed
			}
		}

		return bytes_read;
	}

	task_t *current = scheduler_get_current_task();
	if (current == NULL) {
		return -ESRCH;
	}

	if (fd < 0 || fd >= MAX_OPEN_FILES) {
		return -EBADF;
	}

	vfs_file_t *file = (vfs_file_t *)current->p_p->ps_fd_table[fd].file;
	if (file == NULL) {
		return -EBADF;
	}

	if ((file->f_flags & VFS_O_WRONLY) == VFS_O_WRONLY) {
		return -EBADF;
	}

	ssize_t result = vfs_read(file, buf, count);
	if (result < 0) {
		return -vfs_errno((int)result);
	}

	return result;
}

int64_t
sys_write(int fd, const void *buf, size_t count)
{
	if (count == 0) {
		return 0;
	}

	if (!is_user_range(buf, count)) {
		return -EFAULT;
	}

	if (fd == 1 || fd == 2) {
		const char *str = (const char *)buf;
		for (size_t i = 0; i < count; i++) {
			tty_putchar(str[i]);
		}
		return count;
	}

	task_t *current = scheduler_get_current_task();
	if (current == NULL) {
		return -ESRCH;
	}

	if (fd < 0 || fd >= MAX_OPEN_FILES) {
		return -EBADF;
	}

	vfs_file_t *file = (vfs_file_t *)current->p_p->ps_fd_table[fd].file;
	if (file == NULL) {
		return -EBADF;
	}

	if ((file->f_flags & VFS_O_RDONLY) == VFS_O_RDONLY) {
		return -EBADF;
	}

	ssize_t result = vfs_write(file, buf, count);
	if (result < 0) {
		return -vfs_errno((int)result);
	}

	return result;
}

int64_t
sys_open(const char *path, uint32_t flags, mode_t mode)
{
	task_t *current = scheduler_get_current_task();
	if (current == NULL) {
		return -ESRCH;
	}

	if (!is_user_address(path)) {
		return -EFAULT;
	}

	size_t path_len = 0;
	const char *p = path;
	while (is_user_address(p) && *p != '\0' && path_len < VFS_MAX_PATH) {
		p++;
		path_len++;
	}

	if (path_len == 0) {
		return -EINVAL;
	}

	if (path_len >= VFS_MAX_PATH) {
		return -ENAMETOOLONG;
	}

	int fd = -1;
	for (int i = 3; i < MAX_OPEN_FILES; i++) {
		if (current->p_p->ps_fd_table[i].file == NULL) {
			fd = i;
			break;
		}
	}

	if (fd == -1) {
		return -EMFILE;
	}

	vfs_file_t *file;
	int ret = vfs_open(path, flags, mode, &file);
	if (ret != VFS_SUCCESS) {
		return -vfs_errno(ret);
	}

	current->p_p->ps_fd_table[fd].file = file;
	current->p_p->ps_fd_table[fd].flags = 0;
	return fd;
}

int64_t
sys_close(int fd)
{
	task_t *current = scheduler_get_current_task();
	if (current == NULL) {
		return -ESRCH;
	}

	if (fd < 0 || fd >= MAX_OPEN_FILES) {
		return -EBADF;
	}

	vfs_file_t *file = (vfs_file_t *)current->p_p->ps_fd_table[fd].file;
	if (file == NULL) {
		return -EBADF;
	}

	int ret = vfs_close(file);
	if (ret != VFS_SUCCESS) {
		return -vfs_errno(ret);
	}

	current->p_p->ps_fd_table[fd].file = NULL;
	current->p_p->ps_fd_table[fd].flags = 0;
	return 0;
}

int64_t
sys_creat(const char *path, mode_t mode)
{
	/* creat() is equivalent to open() with O_CREAT|O_WRONLY|O_TRUNC */
	return sys_open(path, O_CREAT | O_WRONLY | O_TRUNC, mode);
}

int64_t
sys_openat(int dirfd, const char *pathname, uint32_t flags, mode_t mode)
{
	task_t *current = scheduler_get_current_task();
	if (current == NULL) {
		return -ESRCH;
	}

	if (!is_user_address(pathname)) {
		return -EFAULT;
	}

	size_t path_len = 0;
	const char *p = pathname;
	while (is_user_address(p) && *p != '\0' && path_len < VFS_MAX_PATH) {
		p++;
		path_len++;
	}

	if (path_len == 0) {
		return -EINVAL;
	}

	if (path_len >= VFS_MAX_PATH) {
		return -ENAMETOOLONG;
	}

	if (pathname[0] == '/') {
		return sys_open(pathname, flags, mode);
	}

	/* Handle AT_FDCWD - use current working directory */
	if (dirfd == AT_FDCWD) {
		/* For now, treat relative paths as if they're from root */
		/* TODO: Implement proper current working directory support */
		tty_printf("[OPENAT] Warning: AT_FDCWD not fully implemented, "
		           "treating as relative to root\n");

		char *abs_path = kmalloc(path_len + 2);
		if (abs_path == NULL) {
			return -ENOMEM;
		}

		abs_path[0] = '/';
		memcpy(abs_path + 1, pathname, path_len + 1);

		int64_t ret = sys_open(abs_path, flags, mode);
		kfree(abs_path);
		return ret;
	}

	if (dirfd < 0 || dirfd >= MAX_OPEN_FILES) {
		return -EBADF;
	}

	vfs_file_t *dir_file = (vfs_file_t *)current->p_p->ps_fd_table[dirfd].file;
	if (dir_file == NULL) {
		return -EBADF;
	}

	vnode_t *dir_vnode = dir_file->f_vnode;
	if ((dir_vnode->v_mode & VFS_IFMT) != VFS_IFDIR) {
		return -ENOTDIR;
	}

	/* TODO: Implement proper path resolution relative to dirfd */

	tty_printf(
	    "[OPENAT] Warning: dirfd-relative paths not fully implemented\n");
	return -ENOTSUP;
}

int64_t
sys_mkdir(const char *path, mode_t mode)
{
	if (!is_user_address(path)) {
		return -EFAULT;
	}

	size_t path_len = 0;
	const char *p = path;
	while (is_user_address(p) && *p != '\0' && path_len < VFS_MAX_PATH) {
		p++;
		path_len++;
	}

	if (path_len == 0) {
		return -EINVAL;
	}

	if (path_len >= VFS_MAX_PATH) {
		return -ENAMETOOLONG;
	}

	int ret = vfs_mkdir(path, mode);
	if (ret != VFS_SUCCESS) {
		return -vfs_errno(ret);
	}

	return 0;
}

int64_t
sys_fcntl(int fd, int cmd, uint64_t arg)
{
	task_t *current = scheduler_get_current_task();
	if (current == NULL) {
		return -ESRCH;
	}

	/* Validate fd range */
	if (fd < 0 || fd >= MAX_OPEN_FILES) {
		return -EBADF;
	}

	fd_entry_t *fd_entry = &current->p_p->ps_fd_table[fd];

	switch (cmd) {
	case F_DUPFD: {
		/* Duplicate fd to lowest available fd >= arg */
		int minfd = (int)arg;
		if (minfd < 0 || minfd >= MAX_OPEN_FILES) {
			return -EINVAL;
		}

		if (fd_entry->file == NULL) {
			return -EBADF;
		}

		/* Find lowest available fd >= minfd */
		int newfd = -1;
		for (int i = minfd; i < MAX_OPEN_FILES; i++) {
			if (current->p_p->ps_fd_table[i].file == NULL) {
				newfd = i;
				break;
			}
		}

		if (newfd == -1) {
			return -EMFILE;
		}

		/* Duplicate the file */
		vfs_file_t *file = (vfs_file_t *)fd_entry->file;
		current->p_p->ps_fd_table[newfd].file = file;
		current->p_p->ps_fd_table[newfd].flags =
		    0; /* FD_CLOEXEC not inherited */

		/* Increment refcount */
		uint64_t flags;
		spinlock_acquire_irqsave(&file->f_lock, &flags);
		file->f_refcount++;
		spinlock_release_irqrestore(&file->f_lock, flags);

		tty_printf(
		    "[FCNTL] F_DUPFD: duplicated fd %d -> fd %d\n", fd, newfd);
		return newfd;
	}

	case F_DUPFD_CLOEXEC: {
		/* Duplicate fd with FD_CLOEXEC set */
		int minfd = (int)arg;
		if (minfd < 0 || minfd >= MAX_OPEN_FILES) {
			return -EINVAL;
		}

		if (fd_entry->file == NULL) {
			return -EBADF;
		}

		/* Find lowest available fd >= minfd */
		int newfd = -1;
		for (int i = minfd; i < MAX_OPEN_FILES; i++) {
			if (current->p_p->ps_fd_table[i].file == NULL) {
				newfd = i;
				break;
			}
		}

		if (newfd == -1) {
			return -EMFILE;
		}

		/* Duplicate the file with CLOEXEC */
		vfs_file_t *file = (vfs_file_t *)fd_entry->file;
		current->p_p->ps_fd_table[newfd].file = file;
		current->p_p->ps_fd_table[newfd].flags = FD_CLOEXEC;

		/* Increment refcount */
		uint64_t flags;
		spinlock_acquire_irqsave(&file->f_lock, &flags);
		file->f_refcount++;
		spinlock_release_irqrestore(&file->f_lock, flags);

		tty_printf(
		    "[FCNTL] F_DUPFD_CLOEXEC: duplicated fd %d -> fd %d\n",
		    fd,
		    newfd);
		return newfd;
	}

	case F_GETFD:
		/* Get file descriptor flags */
		if (fd_entry->file == NULL) {
			return -EBADF;
		}
		return fd_entry->flags;

	case F_SETFD:
		/* Set file descriptor flags */
		if (fd_entry->file == NULL) {
			return -EBADF;
		}
		/* Only FD_CLOEXEC is valid */
		fd_entry->flags = (int)arg & FD_CLOEXEC;
		return 0;

	case F_GETFL: {
		/* Get file status flags */
		if (fd_entry->file == NULL) {
			return -EBADF;
		}

		vfs_file_t *file = (vfs_file_t *)fd_entry->file;

		/* Convert VFS flags to O_* flags */
		int flags = 0;

		/* Access mode */
		if ((file->f_flags & VFS_O_RDWR) == VFS_O_RDWR) {
			flags |= O_RDWR;
		} else if (file->f_flags & VFS_O_WRONLY) {
			flags |= O_WRONLY;
		} else {
			flags |= O_RDONLY;
		}

		/* Other flags */
		if (file->f_flags & VFS_O_APPEND)
			flags |= O_APPEND;
		if (file->f_flags & VFS_O_CREAT)
			flags |= O_CREAT;
		if (file->f_flags & VFS_O_TRUNC)
			flags |= O_TRUNC;
		if (file->f_flags & VFS_O_EXCL)
			flags |= O_EXCL;

		return flags;
	}

	case F_SETFL: {
		/* Set file status flags (only certain flags can be changed) */
		if (fd_entry->file == NULL) {
			return -EBADF;
		}

		vfs_file_t *file = (vfs_file_t *)fd_entry->file;

		/* Only O_APPEND, O_NONBLOCK, O_SYNC can be changed */
		uint64_t flags;
		spinlock_acquire_irqsave(&file->f_lock, &flags);

		/* Clear modifiable flags */
		file->f_flags &= ~(VFS_O_APPEND);

		/* Set new flags */
		if (arg & O_APPEND)
			file->f_flags |= VFS_O_APPEND;
		/* Note: O_NONBLOCK and O_SYNC not implemented in VFS yet */

		spinlock_release_irqrestore(&file->f_lock, flags);
		return 0;
	}

	case F_GETOWN:
		/* Get process/thread ID for SIGIO/SIGURG */
		/* Not implemented - signals not supported yet */
		return -ENOSYS;

	case F_SETOWN:
		/* Set process/thread ID for SIGIO/SIGURG */
		/* Not implemented - signals not supported yet */
		return -ENOSYS;

	case F_GETLK:
	case F_SETLK:
	case F_SETLKW:
		/* File locking - not implemented */
		return -ENOSYS;

	default:
		tty_printf("[FCNTL] Unknown command: %d\n", cmd);
		return -EINVAL;
	}
}

int64_t
sys_dup(int oldfd)
{
	task_t *current = scheduler_get_current_task();
	if (current == NULL) {
		return -ESRCH;
	}

	if (oldfd < 0 || oldfd >= MAX_OPEN_FILES) {
		return -EBADF;
	}

	vfs_file_t *file = (vfs_file_t *)current->p_p->ps_fd_table[oldfd].file;
	if (file == NULL) {
		return -EBADF;
	}

	int newfd = -1;
	for (int i = 0; i < MAX_OPEN_FILES; i++) {
		if (current->p_p->ps_fd_table[i].file == NULL) {
			newfd = i;
			break;
		}
	}

	if (newfd == -1) {
		return -EMFILE;
	}

	current->p_p->ps_fd_table[newfd].file = file;
	current->p_p->ps_fd_table[newfd].flags = 0; /* Don't inherit FD_CLOEXEC */

	uint64_t flags;
	spinlock_acquire_irqsave(&file->f_lock, &flags);
	file->f_refcount++;
	spinlock_release_irqrestore(&file->f_lock, flags);

	tty_printf("[DUP] Duplicated fd %d -> fd %d\n", oldfd, newfd);
	return newfd;
}

int64_t
sys_dup2(int oldfd, int newfd)
{
	task_t *current = scheduler_get_current_task();
	if (current == NULL) {
		return -ESRCH;
	}

	if (oldfd < 0 || oldfd >= MAX_OPEN_FILES) {
		return -EBADF;
	}

	vfs_file_t *file = (vfs_file_t *)current->p_p->ps_fd_table[oldfd].file;
	if (file == NULL) {
		return -EBADF;
	}

	if (newfd < 0 || newfd >= MAX_OPEN_FILES) {
		return -EBADF;
	}

	if (oldfd == newfd) {
		return newfd;
	}

	if (current->p_p->ps_fd_table[newfd].file != NULL) {
		vfs_file_t *old_file =
		    (vfs_file_t *)current->p_p->ps_fd_table[newfd].file;
		vfs_close(old_file);
	}

	current->p_p->ps_fd_table[newfd].file = file;
	current->p_p->ps_fd_table[newfd].flags = 0; /* Don't inherit FD_CLOEXEC */

	uint64_t flags;
	spinlock_acquire_irqsave(&file->f_lock, &flags);
	file->f_refcount++;
	spinlock_release_irqrestore(&file->f_lock, flags);

	tty_printf("[DUP2] Duplicated fd %d -> fd %d\n", oldfd, newfd);
	return newfd;
}

int64_t
sys_stat(const char *path, struct stat *statbuf)
{
	if (path == NULL || statbuf == NULL) {
		return -EINVAL;
	}

	if (!is_user_address(path)) {
		return -EFAULT;
	}

	size_t path_len = 0;
	const char *p = path;
	while (is_user_address(p) && *p != '\0' && path_len < VFS_MAX_PATH) {
		p++;
		path_len++;
	}

	if (path_len == 0) {
		return -EINVAL;
	}

	if (path_len >= VFS_MAX_PATH) {
		return -ENAMETOOLONG;
	}

	if (!is_user_range(statbuf, sizeof(struct stat))) {
		return -EFAULT;
	}

	vfs_stat_t kstat;
	memset(&kstat, 0, sizeof(vfs_stat_t));

	int ret = vfs_stat(path, &kstat);
	if (ret != VFS_SUCCESS) {
		return -vfs_errno(ret);
	}

	statbuf->st_dev = kstat.st_dev;
	statbuf->st_ino = kstat.st_ino;
	statbuf->st_mode = kstat.st_mode;
	statbuf->st_nlink = kstat.st_nlink;
	statbuf->st_uid = kstat.st_uid;
	statbuf->st_gid = kstat.st_gid;
	statbuf->st_rdev = kstat.st_rdev;
	statbuf->st_size = kstat.st_size;
	statbuf->st_blksize = kstat.st_blksize;
	statbuf->st_blocks = kstat.st_blocks;
	statbuf->st_atime = kstat.st_atime;
	statbuf->st_mtime = kstat.st_mtime;
	statbuf->st_ctime = kstat.st_ctime;

	return 0;
}

int64_t
sys_fstat(int fd, struct stat *statbuf)
{
	if (statbuf == NULL) {
		return -EINVAL;
	}

	if (!is_user_range(statbuf, sizeof(struct stat))) {
		return -EFAULT;
	}

	task_t *current = scheduler_get_current_task();
	if (current == NULL) {
		return -ESRCH;
	}

	if (fd < 0 || fd >= MAX_OPEN_FILES) {
		return -EBADF;
	}

	vfs_file_t *file = (vfs_file_t *)current->p_p->ps_fd_table[fd].file;
	if (file == NULL) {
		return -EBADF;
	}

	vfs_stat_t kstat;
	memset(&kstat, 0, sizeof(vfs_stat_t));

	int ret = vfs_fstat(file, &kstat);
	if (ret != VFS_SUCCESS) {
		return -vfs_errno(ret);
	}

	statbuf->st_dev = kstat.st_dev;
	statbuf->st_ino = kstat.st_ino;
	statbuf->st_mode = kstat.st_mode;
	statbuf->st_nlink = kstat.st_nlink;
	statbuf->st_uid = kstat.st_uid;
	statbuf->st_gid = kstat.st_gid;
	statbuf->st_rdev = kstat.st_rdev;
	statbuf->st_size = kstat.st_size;
	statbuf->st_blksize = kstat.st_blksize;
	statbuf->st_blocks = kstat.st_blocks;
	statbuf->st_atime = kstat.st_atime;
	statbuf->st_mtime = kstat.st_mtime;
	statbuf->st_ctime = kstat.st_ctime;

	return 0;
}

int64_t
sys_lstat(const char *path, struct stat *statbuf)
{
	if (path == NULL || statbuf == NULL) {
		return -EINVAL;
	}

	if (!is_user_address(path)) {
		return -EFAULT;
	}

	size_t path_len = 0;
	const char *p = path;
	while (is_user_address(p) && *p != '\0' && path_len < VFS_MAX_PATH) {
		p++;
		path_len++;
	}

	if (path_len == 0) {
		return -EINVAL;
	}

	if (path_len >= VFS_MAX_PATH) {
		return -ENAMETOOLONG;
	}

	if (!is_user_range(statbuf, sizeof(struct stat))) {
		return -EFAULT;
	}

	vfs_stat_t kstat;
	memset(&kstat, 0, sizeof(vfs_stat_t));

	int ret = vfs_lstat(path, &kstat);
	if (ret != VFS_SUCCESS) {
		return -vfs_errno(ret);
	}

	statbuf->st_dev = kstat.st_dev;
	statbuf->st_ino = kstat.st_ino;
	statbuf->st_mode = kstat.st_mode;
	statbuf->st_nlink = kstat.st_nlink;
	statbuf->st_uid = kstat.st_uid;
	statbuf->st_gid = kstat.st_gid;
	statbuf->st_rdev = kstat.st_rdev;
	statbuf->st_size = kstat.st_size;
	statbuf->st_blksize = kstat.st_blksize;
	statbuf->st_blocks = kstat.st_blocks;
	statbuf->st_atime = kstat.st_atime;
	statbuf->st_mtime = kstat.st_mtime;
	statbuf->st_ctime = kstat.st_ctime;

	return 0;
}

int64_t
sys_lseek(int fd, off_t offset, int whence)
{
	task_t *current = scheduler_get_current_task();
	if (current == NULL) {
		return -ESRCH;
	}

	if (fd < 0 || fd >= MAX_OPEN_FILES) {
		return -EBADF;
	}

	vfs_file_t *file = (vfs_file_t *)current->p_p->ps_fd_table[fd].file;
	if (file == NULL) {
		return -EBADF;
	}

	if (whence != VFS_SEEK_SET && whence != VFS_SEEK_CUR &&
	    whence != VFS_SEEK_END) {
		return -EINVAL;
	}

	off_t result = vfs_seek(file, offset, whence);

	if (result < 0) {
		return -vfs_errno((int)result);
	}

	return result;
}

int64_t
sys_fork(syscall_registers_t *regs)
{
	struct proc *parent_proc = proc_get_current();
	if (!parent_proc || !parent_proc->p_p) {
		return -ESRCH;
	}

	struct process *parent_ps = parent_proc->p_p;

	tty_printf("[FORK] Process %d (%s) calling fork()\n",
	           parent_ps->ps_pid,
	           parent_ps->ps_comm);

	struct process *child_ps = process_alloc(parent_ps->ps_comm);
	if (!child_ps) {
		tty_printf("[FORK] Failed to allocate child process\n");
		return -ENOMEM;
	}

	struct proc *child_proc = proc_alloc(child_ps, parent_proc->p_name);
	if (!child_proc) {
		tty_printf("[FORK] Failed to allocate child thread\n");
		process_free(child_ps);
		return -ENOMEM;
	}

	child_ps->ps_pptr = parent_ps;
	LIST_INSERT_HEAD(&parent_ps->ps_children, child_ps, ps_sibling);

	extern struct limine_hhdm_response *boot_get_hhdm(void);
	struct limine_hhdm_response *hhdm = boot_get_hhdm();
	uint64_t hhdm_offset = hhdm ? hhdm->offset : 0;

	page_directory_t *parent_pd = parent_ps->ps_vmspace;
	page_directory_t *child_pd = child_ps->ps_vmspace;

	pml4e_t *parent_pml4 = parent_pd->pml4;
	uint64_t pages_copied = 0;

	for (int pml4_idx = 0; pml4_idx < 256;
	     pml4_idx++) { /* Only user space */
		if (!(parent_pml4[pml4_idx] & PAGE_PRESENT))
			continue;

		uint64_t pdpt_phys = parent_pml4[pml4_idx] & PAGE_ADDR_MASK;
		pdpte_t *pdpt = (pdpte_t *)mmu_phys_to_virt(pdpt_phys);

		for (int pdpt_idx = 0; pdpt_idx < 512; pdpt_idx++) {
			if (!(pdpt[pdpt_idx] & PAGE_PRESENT))
				continue;
			if (pdpt[pdpt_idx] & PAGE_HUGE)
				continue;

			uint64_t pd_phys = pdpt[pdpt_idx] & PAGE_ADDR_MASK;
			pde_t *pd_table = (pde_t *)mmu_phys_to_virt(pd_phys);

			for (int pd_idx = 0; pd_idx < 512; pd_idx++) {
				if (!(pd_table[pd_idx] & PAGE_PRESENT))
					continue;
				if (pd_table[pd_idx] & PAGE_HUGE)
					continue;

				uint64_t pt_phys =
				    pd_table[pd_idx] & PAGE_ADDR_MASK;
				pte_t *pt = (pte_t *)mmu_phys_to_virt(pt_phys);

				for (int pt_idx = 0; pt_idx < 512; pt_idx++) {
					if (!(pt[pt_idx] & PAGE_PRESENT))
						continue;

					uint64_t src_phys =
					    pt[pt_idx] & PAGE_ADDR_MASK;
					uint64_t flags =
					    pt[pt_idx] & ~PAGE_ADDR_MASK;

					void *new_page = pmm_alloc();
					if (!new_page) {
						tty_printf(
						    "[FORK] Out of memory "
						    "during fork\n");
						/* TODO: Clean up partial copy
						 */
						proc_free(child_proc);
						process_free(child_ps);
						return -ENOMEM;
					}

					void *src_page =
					    (void *)(src_phys + hhdm_offset);
					memcpy(new_page, src_page, PAGE_SIZE);

					uint64_t new_phys =
					    (uint64_t)new_page - hhdm_offset;
					uint64_t virt =
					    ((uint64_t)pml4_idx << PML4_SHIFT) |
					    ((uint64_t)pdpt_idx << PDPT_SHIFT) |
					    ((uint64_t)pd_idx << PD_SHIFT) |
					    ((uint64_t)pt_idx << PT_SHIFT);

					if (!mmu_map_page(child_pd,
					                  virt,
					                  new_phys,
					                  flags)) {
						pmm_free(new_page);
						/* TODO: Clean up partial copy
						 */
						proc_free(child_proc);
						process_free(child_ps);
						return -ENOMEM;
					}

					pages_copied++;
				}
			}
		}
	}

	tty_printf("[FORK] Copied %llu pages (%llu KB)\n",
	           pages_copied,
	           pages_copied * 4);

	child_ps->ps_brk = parent_ps->ps_brk;
	child_ps->ps_flags = (parent_ps->ps_flags & ~PS_EMBRYO);

	struct trapframe *child_tf = (struct trapframe *)pmm_alloc();
	if (!child_tf) {
		proc_free(child_proc);
		process_free(child_ps);
		return -ENOMEM;
	}

	memset(child_tf, 0, PAGE_SIZE);

	child_tf->tf_rax = 0; /* Child returns 0 from fork() */
	child_tf->tf_rbx = regs->rbx;
	child_tf->tf_rcx = regs->rcx;
	child_tf->tf_rdx = regs->rdx;
	child_tf->tf_rsi = regs->rsi;
	child_tf->tf_rdi = regs->rdi;
	child_tf->tf_rbp = regs->rbp;
	child_tf->tf_r8 = regs->r8;
	child_tf->tf_r9 = regs->r9;
	child_tf->tf_r10 = regs->r10;
	child_tf->tf_r11 = regs->r11;
	child_tf->tf_r12 = regs->r12;
	child_tf->tf_r13 = regs->r13;
	child_tf->tf_r14 = regs->r14;
	child_tf->tf_r15 = regs->r15;
	child_tf->tf_rip = regs->rip;
	child_tf->tf_cs = regs->cs;
	child_tf->tf_rflags = regs->rflags;
	child_tf->tf_rsp = regs->userrsp;
	child_tf->tf_ss = regs->ss;
	child_tf->tf_trapno = 0;
	child_tf->tf_err = 0;

	child_proc->p_md.md_regs = child_tf;
	child_proc->p_md.md_flags = 0;

	child_proc->p_stat = SRUN;
	child_ps->ps_flags &= ~PS_EMBRYO;

	task_t *child_task = scheduler_create_forked_task(child_proc);
	if (!child_task) {
		tty_printf("[FORK] Failed to create scheduler task\n");
		pmm_free(child_tf);
		proc_free(child_proc);
		process_free(child_ps);
		return -ENOMEM;
	}

	if (!scheduler_add_forked_task(child_task)) {
		tty_printf("[FORK] Failed to add task to scheduler\n");
		pmm_free(child_task);
		pmm_free(child_tf);
		proc_free(child_proc);
		process_free(child_ps);
		return -ENOMEM;
	}

	tty_printf("[FORK] Created child process %d (task %d) from parent %d\n",
	           child_ps->ps_pid,
	           child_task->p_tid,
	           parent_ps->ps_pid);
	tty_printf("[FORK] Child will return to RIP=0x%lx RSP=0x%lx\n",
	           child_tf->tf_rip,
	           child_tf->tf_rsp);

	return (int64_t)child_ps->ps_pid;
}

int64_t
sys_getpid(void)
{
	struct proc *p = proc_get_current();
	if (p == NULL || p->p_p == NULL) {
		return -ESRCH;
	}

	return (int64_t)p->p_p->ps_pid;
}

int64_t
sys_getppid(void)
{
	struct proc *p = proc_get_current();
	if (p == NULL || p->p_p == NULL) {
		return -ESRCH;
	}

	struct process *ps = p->p_p;

	if (ps->ps_pptr == NULL) {
		return 0;
	}

	return (int64_t)ps->ps_pptr->ps_pid;
}

void *
sys_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
	struct proc *p = proc_get_current();

	tty_printf("[MMAP DEBUG] Called: addr=%p len=%lu prot=%d flags=%d\n",
	           addr,
	           (unsigned long)length,
	           prot,
	           flags);

	if (p == NULL) {
		tty_printf("[MMAP DEBUG] No current proc!\n");
		return (void *)(intptr_t)-EINVAL;
	}

	struct process *ps = p->p_p;
	if (ps == NULL || ps->ps_vmspace == NULL) {
		tty_printf("[MMAP DEBUG] No process or vmspace!\n");
		return (void *)(intptr_t)-EINVAL;
	}

	tty_printf("[MMAP DEBUG] Process found: pid=%d\n", ps->ps_pid);

	if (length == 0) {
		return (void *)(intptr_t)-EINVAL;
	}

	if (!(flags & MAP_ANONYMOUS)) {
		return (void *)(intptr_t)-EINVAL;
	}

	if (!(flags & MAP_SHARED) && !(flags & MAP_PRIVATE)) {
		return (void *)(intptr_t)-EINVAL;
	}

	size_t aligned_length = (length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	size_t num_pages = aligned_length / PAGE_SIZE;
	uint64_t virt_addr;

	if (flags & MAP_FIXED) {
		if (!is_user_address(addr)) {
			return (void *)(intptr_t)-EINVAL;
		}
		virt_addr = (uint64_t)addr & ~(PAGE_SIZE - 1);
	} else {
		virt_addr = (ps->ps_brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

		if (addr != NULL && is_user_address(addr)) {
			uint64_t hint = (uint64_t)addr & ~(PAGE_SIZE - 1);
			if (hint >= virt_addr) {
				virt_addr = hint;
			}
		}
	}

	tty_printf("[MMAP DEBUG] Mapping at virt=0x%lx, %lu pages\n",
	           virt_addr,
	           (unsigned long)num_pages);

	uint64_t page_flags = PAGE_PRESENT | PAGE_USER;

	if (prot & PROT_WRITE) {
		page_flags |= PAGE_WRITE;
	}

	if (!(prot & PROT_EXEC)) {
		page_flags |= PAGE_NX;
	}

	extern struct limine_hhdm_response *boot_get_hhdm(void);
	struct limine_hhdm_response *hhdm = boot_get_hhdm();
	uint64_t hhdm_offset = hhdm ? hhdm->offset : 0;

	for (size_t i = 0; i < num_pages; i++) {
		uint64_t virt_page = virt_addr + (i * PAGE_SIZE);

		void *page_virt = pmm_alloc();
		if (page_virt == NULL) {
			tty_printf("[MMAP DEBUG] Failed to allocate page %lu\n",
			           (unsigned long)i);
			for (size_t j = 0; j < i; j++) {
				paging_unmap_range(ps->ps_vmspace,
				                   virt_addr + (j * PAGE_SIZE),
				                   1);
			}
			return (void *)(intptr_t)-EINVAL;
		}

		memset(page_virt, 0, PAGE_SIZE);

		uint64_t phys_page = (uint64_t)page_virt - hhdm_offset;

		if (!paging_map_range(
		        ps->ps_vmspace, virt_page, phys_page, 1, page_flags)) {
			tty_printf("[MMAP DEBUG] Failed to map page at 0x%lx\n",
			           virt_page);
			pmm_free(page_virt);
			for (size_t j = 0; j < i; j++) {
				paging_unmap_range(ps->ps_vmspace,
				                   virt_addr + (j * PAGE_SIZE),
				                   1);
			}
			return (void *)(intptr_t)-EINVAL;
		}

		tty_printf(
		    "[MMAP DEBUG] Mapped page %lu: virt=0x%lx phys=0x%lx\n",
		    (unsigned long)i,
		    virt_page,
		    phys_page);
	}

	if (virt_addr + aligned_length > ps->ps_brk) {
		ps->ps_brk = virt_addr + aligned_length;
	}

	tty_printf("[MMAP DEBUG] Success! Returning 0x%lx\n", virt_addr);

	return (void *)virt_addr;
}

int64_t
sys_munmap(void *addr, size_t length)
{
	struct proc *p = proc_get_current();
	if (p == NULL) {
		return -ESRCH;
	}

	struct process *ps = p->p_p;
	if (ps == NULL || ps->ps_vmspace == NULL) {
		return -ESRCH;
	}

	if (!is_user_address(addr)) {
		return -EINVAL;
	}

	if (length == 0) {
		return -EINVAL;
	}

	uint64_t virt_addr = (uint64_t)addr & ~(PAGE_SIZE - 1);
	size_t aligned_length = (length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	size_t num_pages = aligned_length / PAGE_SIZE;

	tty_printf("[MUNMAP DEBUG] Unmapping virt=0x%lx, %lu pages\n",
	           virt_addr,
	           (unsigned long)num_pages);

	extern struct limine_hhdm_response *boot_get_hhdm(void);
	struct limine_hhdm_response *hhdm = boot_get_hhdm();
	uint64_t hhdm_offset = hhdm ? hhdm->offset : 0;

	for (size_t i = 0; i < num_pages; i++) {
		uint64_t virt_page = virt_addr + (i * PAGE_SIZE);

		uint64_t phys_addr =
		    mmu_get_physical_address(ps->ps_vmspace, virt_page);

		paging_unmap_range(ps->ps_vmspace, virt_page, 1);

		if (phys_addr != 0) {
			void *page_virt = (void *)(phys_addr + hhdm_offset);
			pmm_free(page_virt);
		}
	}

	tty_printf("[MUNMAP DEBUG] Success!\n");

	return 0;
}

int64_t
sys_mprotect(void *addr, size_t len, int prot)
{
	struct proc *p = proc_get_current();
	if (p == NULL) {
		return -ESRCH;
	}

	struct process *ps = p->p_p;
	if (ps == NULL || ps->ps_vmspace == NULL) {
		return -ESRCH;
	}

	if (!is_user_address(addr)) {
		return -EINVAL;
	}

	if (len == 0) {
		return -EINVAL;
	}

	uint64_t virt_addr = (uint64_t)addr & ~(PAGE_SIZE - 1);
	size_t aligned_length = (len + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	size_t num_pages = aligned_length / PAGE_SIZE;

	uint64_t page_flags = PAGE_PRESENT | PAGE_USER;

	if (prot & PROT_WRITE) {
		page_flags |= PAGE_WRITE;
	}

	if (!(prot & PROT_EXEC)) {
		page_flags |= PAGE_NX;
	}

	if (!paging_protect_range(
	        ps->ps_vmspace, virt_addr, num_pages, page_flags)) {
		return -ENOMEM;
	}

	return 0;
}

int64_t
sys_brk(void *addr)
{
	struct proc *p = proc_get_current();
	if (p == NULL) {
		return -ESRCH;
	}

	struct process *ps = p->p_p;
	if (ps == NULL || ps->ps_vmspace == NULL) {
		return -ESRCH;
	}

	tty_printf("[BRK] Called with addr=0x%lx, current brk=0x%lx\n",
	           (uint64_t)addr,
	           ps->ps_brk);

	if (addr == NULL || addr == 0) {
		return (int64_t)ps->ps_brk;
	}

	uint64_t new_brk = (uint64_t)addr;

	if (!is_user_address(addr)) {
		tty_printf("[BRK] Invalid address (not in user space)\n");
		return -ENOMEM;
	}

	if (new_brk < USER_CODE_BASE ||
	    new_brk >= (USER_STACK_TOP - PROCESS_USER_STACK_SIZE)) {
		tty_printf("[BRK] Address out of valid range\n");
		return -ENOMEM;
	}

	uint64_t old_brk = ps->ps_brk;

	if (old_brk == 0) {
		old_brk = (USER_CODE_BASE + 0x100000) &
		          ~(PAGE_SIZE - 1); /* Start heap 1MB after code */
		ps->ps_brk = old_brk;
		tty_printf("[BRK] Initializing brk to 0x%lx\n", old_brk);
	}

	if (new_brk == old_brk) {
		return (int64_t)old_brk;
	}

	extern struct limine_hhdm_response *boot_get_hhdm(void);
	struct limine_hhdm_response *hhdm = boot_get_hhdm();
	uint64_t hhdm_offset = hhdm ? hhdm->offset : 0;

	if (new_brk > old_brk) {
		uint64_t old_page =
		    (old_brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
		uint64_t new_page =
		    (new_brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

		tty_printf("[BRK] Expanding from 0x%lx to 0x%lx\n",
		           old_page,
		           new_page);

		uint64_t current_page = old_page;
		while (current_page < new_page) {
			uint64_t phys_addr = mmu_get_physical_address(
			    ps->ps_vmspace, current_page);

			if (phys_addr == 0) {
				void *page_virt = pmm_alloc();
				if (page_virt == NULL) {
					tty_printf("[BRK] Out of memory\n");
					return -ENOMEM;
				}

				memset(page_virt, 0, PAGE_SIZE);
				uint64_t phys_page =
				    (uint64_t)page_virt - hhdm_offset;

				uint64_t flags =
				    PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
				if (!paging_map_range(ps->ps_vmspace,
				                      current_page,
				                      phys_page,
				                      1,
				                      flags)) {
					tty_printf("[BRK] Failed to map page "
					           "at 0x%lx\n",
					           current_page);
					pmm_free(page_virt);
					return -ENOMEM;
				}

				tty_printf("[BRK] Mapped new page: virt=0x%lx "
				           "phys=0x%lx\n",
				           current_page,
				           phys_page);
			}

			current_page += PAGE_SIZE;
		}

		ps->ps_brk = new_brk;
		tty_printf("[BRK] New brk set to 0x%lx\n", new_brk);
		return (int64_t)new_brk;
	}

	else {
		uint64_t new_page =
		    (new_brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
		uint64_t old_page =
		    (old_brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

		tty_printf("[BRK] Shrinking from 0x%lx to 0x%lx\n",
		           old_page,
		           new_page);

		uint64_t current_page = new_page;
		while (current_page < old_page) {
			uint64_t phys_addr = mmu_get_physical_address(
			    ps->ps_vmspace, current_page);

			if (phys_addr != 0) {
				paging_unmap_range(
				    ps->ps_vmspace, current_page, 1);

				void *page_virt =
				    (void *)(phys_addr + hhdm_offset);
				pmm_free(page_virt);

				tty_printf("[BRK] Unmapped page: virt=0x%lx "
				           "phys=0x%lx\n",
				           current_page,
				           phys_addr);
			}

			current_page += PAGE_SIZE;
		}

		ps->ps_brk = new_brk;
		tty_printf("[BRK] New brk set to 0x%lx\n", new_brk);
		return (int64_t)new_brk;
	}
}

int64_t
sys_gethostname(char *name, size_t len)
{
	if (len == 0) {
		return -EINVAL;
	}

	if (!is_user_range(name, len)) {
		return -EFAULT;
	}

	int ret = gethostname(name, len);
	return (ret < 0) ? ret : 0;
}

int64_t
sys_gethostid(void)
{
	return gethostid();
}

int64_t
sys_sysinfo(struct sysinfo *info)
{
	if (!is_user_range(info, sizeof(struct sysinfo))) {
		return -EFAULT;
	}

	int ret = kern_sysinfo(info);
	return (ret < 0) ? ret : 0;
}

int64_t
sys_uname(struct utsname *buf)
{
	if (!is_user_range(buf, sizeof(struct utsname))) {
		return -EFAULT;
	}

	extern int kern_uname(struct utsname * name);
	int ret = kern_uname(buf);
	return (ret < 0) ? ret : 0;
}

void
syscall_handler(syscall_registers_t *regs)
{
	uint64_t syscall_num = regs->rax;
	int64_t ret = -ENOSYS;

	switch (syscall_num) {
	case SYSCALL_EXIT:
		sys_exit((int)regs->rdi);
		break;

	case SYSCALL_READ:
		ret = sys_read(
		    (int)regs->rdi, (void *)regs->rsi, (size_t)regs->rdx);
		break;

	case SYSCALL_WRITE:
		ret = sys_write(
		    (int)regs->rdi, (const void *)regs->rsi, (size_t)regs->rdx);
		break;

	case SYSCALL_OPEN:
		ret = sys_open((const char *)regs->rdi,
		               (uint32_t)regs->rsi,
		               (mode_t)regs->rdx);
		break;

	case SYSCALL_CLOSE:
		ret = sys_close((int)regs->rdi);
		break;

	case SYSCALL_CREAT:
		ret = sys_creat((const char *)regs->rdi, (mode_t)regs->rsi);
		break;

	case SYSCALL_OPENAT:
		ret = sys_openat((int)regs->rdi,
		                 (const char *)regs->rsi,
		                 (uint32_t)regs->rdx,
		                 (mode_t)regs->r10);
		break;

	case SYSCALL_MKDIR:
		ret = sys_mkdir((const char *)regs->rdi, (mode_t)regs->rsi);
		break;

	case SYSCALL_FCNTL:
		ret = sys_fcntl((int)regs->rdi, (int)regs->rsi, regs->rdx);
		break;

	case SYSCALL_DUP:
		ret = sys_dup((int)regs->rdi);
		break;

	case SYSCALL_DUP2:
		ret = sys_dup2((int)regs->rdi, (int)regs->rsi);
		break;

	case SYSCALL_STAT:
		ret =
		    sys_stat((const char *)regs->rdi, (struct stat *)regs->rsi);
		break;

	case SYSCALL_FSTAT:
		ret = sys_fstat((int)regs->rdi, (struct stat *)regs->rsi);
		break;

	case SYSCALL_LSTAT:
		ret = sys_lstat((const char *)regs->rdi,
		                (struct stat *)regs->rsi);
		break;

	case SYSCALL_FORK:
		ret = sys_fork(regs);
		break;

	case SYSCALL_BRK:
		ret = sys_brk((void *)regs->rdi);
		break;

	case SYSCALL_GETPID:
		ret = sys_getpid();
		break;

	case SYSCALL_GETPPID:
		ret = sys_getppid();
		break;

		case SYSCALL_MMAP: {
		void *result;

		result = sys_mmap(
		    (void *)regs->rdi,
		    (size_t)regs->rsi,
		    (int)regs->rdx,
		    (int)regs->r10,
		    (int)regs->r8,
		    (off_t)regs->r9);

		if (result == MAP_FAILED) {
			ret = -ENOMEM;
			break;
		}

		ret = (int64_t)result;
		break;
	}

	case SYSCALL_MUNMAP:
		ret = sys_munmap((void *)regs->rdi, (size_t)regs->rsi);
		break;

	case SYSCALL_MPROTECT:
		ret = sys_mprotect(
		    (void *)regs->rdi, (size_t)regs->rsi, (int)regs->rdx);
		break;

	case SYSCALL_GETHOSTNAME:
		ret = sys_gethostname((char *)regs->rdi, (size_t)regs->rsi);
		break;

	case SYSCALL_GETHOSTID:
		ret = sys_gethostid();
		break;

	case SYSCALL_SYSINFO:
		ret = sys_sysinfo((struct sysinfo *)regs->rdi);
		break;

	case SYSCALL_UNAME:
		ret = sys_uname((struct utsname *)regs->rdi);
		break;

	default:
		tty_printf("[SYSCALL] Unknown syscall: %d\n", (int)syscall_num);
		ret = -ENOSYS;
		break;
	}

	regs->rax = (uint64_t)ret;
}

void
syscall_init(void)
{
	idt_set_gate(SYSCALL_INT,
	             (uint64_t)syscall_stub,
	             GDT_SELECTOR_KERNEL_CODE,
	             IDT_GATE_USER_INT,
	             0);
	tty_printf("Syscall handler initialized on INT 0x80\n");
}