#ifndef _SYSCALL_UTILS_H_
#define _SYSCALL_UTILS_H_

#include <sys/errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <vmm.h>
#include <mmu.h>
#include <proc.h>
#include <spinlock.h>

typedef struct vfs_file vfs_file_t;

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

	/* Check for overflow */
	if (end < start || end > USER_SPACE_END)
		return false;

	return is_user_address(addr);
}

static inline bool
is_user_mapped(const void *addr)
{
	struct proc *p = proc_get_current();
	if (!p || !p->p_p || !p->p_p->ps_vmspace)
		return false;

	if (!is_user_address(addr))
		return false;

	/* Check if page is actually mapped */
	uint64_t phys =
	    mmu_get_physical_address(p->p_p->ps_vmspace, (uint64_t)addr);
	return (phys != 0);
}

static inline int
copyin(const void *uaddr, void *kaddr, size_t len)
{
	if (len == 0)
		return 0;

	if (!is_user_range(uaddr, len))
		return -EFAULT;

	struct proc *p = proc_get_current();
	if (!p || !p->p_p || !p->p_p->ps_vmspace)
		return -EFAULT;

	/* Verify pages are mapped (sample at page boundaries) */
	const uint8_t *src = (const uint8_t *)uaddr;
	for (size_t i = 0; i < len; i += PAGE_SIZE) {
		if (!is_user_mapped(src + i))
			return -EFAULT;
	}
	/* Check last byte if not page-aligned */
	if (len > 0 && (len % PAGE_SIZE) != 0) {
		if (!is_user_mapped(src + len - 1))
			return -EFAULT;
	}

	memcpy(kaddr, uaddr, len);
	return 0;
}

static inline int
copyout(const void *kaddr, void *uaddr, size_t len)
{
	if (len == 0)
		return 0;

	if (!is_user_range(uaddr, len))
		return -EFAULT;

	struct proc *p = proc_get_current();
	if (!p || !p->p_p || !p->p_p->ps_vmspace)
		return -EFAULT;

	/* Verify pages are mapped and writable */
	uint8_t *dst = (uint8_t *)uaddr;
	for (size_t i = 0; i < len; i += PAGE_SIZE) {
		if (!is_user_mapped(dst + i))
			return -EFAULT;
	}
	if (len > 0 && (len % PAGE_SIZE) != 0) {
		if (!is_user_mapped(dst + len - 1))
			return -EFAULT;
	}

	memcpy(uaddr, kaddr, len);
	return 0;
}

static inline int
copyinstr(const void *uaddr, void *kaddr, size_t maxlen, size_t *done)
{
	if (maxlen == 0)
		return -ENAMETOOLONG;

	if (!is_user_address(uaddr))
		return -EFAULT;

	struct proc *p = proc_get_current();
	if (!p || !p->p_p || !p->p_p->ps_vmspace)
		return -EFAULT;

	const char *src = (const char *)uaddr;
	char *dst = (char *)kaddr;
	size_t copied = 0;

	while (copied < maxlen) {
		/* Check every page boundary */
		if ((copied % PAGE_SIZE) == 0 || copied == 0) {
			if (!is_user_mapped(src + copied))
				return -EFAULT;
		}

		dst[copied] = src[copied];
		if (dst[copied] == '\0') {
			if (done)
				*done = copied + 1;
			return 0;
		}
		copied++;
	}

	/* String too long */
	return -ENAMETOOLONG;
}

static inline ssize_t
validate_user_string(const char *str, size_t maxlen)
{
	if (!is_user_address(str))
		return -EFAULT;

	struct proc *p = proc_get_current();
	if (!p || !p->p_p || !p->p_p->ps_vmspace)
		return -EFAULT;

	size_t len = 0;
	while (len < maxlen) {
		if ((len % PAGE_SIZE) == 0) {
			if (!is_user_mapped(str + len))
				return -EFAULT;
		}

		if (str[len] == '\0')
			return len;
		len++;
	}

	return -ENAMETOOLONG;
}

static inline int
fd_alloc(struct process *ps, int minfd, void *file, int flags)
{
	if (!ps || minfd < 0 || minfd >= MAX_OPEN_FILES)
		return -EINVAL;

	uint64_t lock_flags;
	spinlock_acquire_irqsave(&ps->ps_lock, &lock_flags);

	int fd = -1;
	for (int i = minfd; i < MAX_OPEN_FILES; i++) {
		if (ps->ps_fd_table[i].file == NULL) {
			fd = i;
			ps->ps_fd_table[i].file = file;
			ps->ps_fd_table[i].flags = flags;
			break;
		}
	}

	spinlock_release_irqrestore(&ps->ps_lock, lock_flags);

	return (fd >= 0) ? fd : -EMFILE;
}

static inline void *
fd_getfile(struct process *ps, int fd)
{
	if (!ps || fd < 0 || fd >= MAX_OPEN_FILES)
		return NULL;

	uint64_t lock_flags;
	spinlock_acquire_irqsave(&ps->ps_lock, &lock_flags);
	void *file = ps->ps_fd_table[fd].file;
	spinlock_release_irqrestore(&ps->ps_lock, lock_flags);

	return file;
}

static inline int
fd_close(struct process *ps, int fd)
{
	if (!ps || fd < 0 || fd >= MAX_OPEN_FILES)
		return -EBADF;

	uint64_t lock_flags;
	spinlock_acquire_irqsave(&ps->ps_lock, &lock_flags);

	if (ps->ps_fd_table[fd].file == NULL) {
		spinlock_release_irqrestore(&ps->ps_lock, lock_flags);
		return -EBADF;
	}

	void *file = ps->ps_fd_table[fd].file;
	ps->ps_fd_table[fd].file = NULL;
	ps->ps_fd_table[fd].flags = 0;

	spinlock_release_irqrestore(&ps->ps_lock, lock_flags);

	/* Close the file outside the lock */
	extern int vfs_close(vfs_file_t *file);
	return vfs_close(file);
}

static inline int
fd_getflags(struct process *ps, int fd)
{
	if (!ps || fd < 0 || fd >= MAX_OPEN_FILES)
		return -EBADF;

	uint64_t lock_flags;
	spinlock_acquire_irqsave(&ps->ps_lock, &lock_flags);

	if (ps->ps_fd_table[fd].file == NULL) {
		spinlock_release_irqrestore(&ps->ps_lock, lock_flags);
		return -EBADF;
	}

	int flags = ps->ps_fd_table[fd].flags;
	spinlock_release_irqrestore(&ps->ps_lock, lock_flags);

	return flags;
}

static inline int
fd_setflags(struct process *ps, int fd, int flags)
{
	if (!ps || fd < 0 || fd >= MAX_OPEN_FILES)
		return -EBADF;

	uint64_t lock_flags;
	spinlock_acquire_irqsave(&ps->ps_lock, &lock_flags);

	if (ps->ps_fd_table[fd].file == NULL) {
		spinlock_release_irqrestore(&ps->ps_lock, lock_flags);
		return -EBADF;
	}

	ps->ps_fd_table[fd].flags = flags;
	spinlock_release_irqrestore(&ps->ps_lock, lock_flags);

	return 0;
}

static inline int
vfs_errno(int vfs_ret)
{
	if (vfs_ret == 0)
		return 0;

	/* VFS returns negative errno values */
	if (vfs_ret < 0)
		return -vfs_ret;

	/* Positive values are also errors in VFS */
	return vfs_ret;
}

#endif /* _SYSCALL_UTILS_H_ */
