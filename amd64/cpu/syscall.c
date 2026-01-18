#include <sys/sysinfo.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <syscall.h>
#include <syscall_utils.h>
#include <scheduler.h>
#include <vfs.h>
#include <idt.h>
#include <fcntl.h>
#include <gdt.h>
#include <vmm.h>
#include <pmm.h>
#include <mmu.h>
#include <paging.h>
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
extern struct limine_hhdm_response *boot_get_hhdm(void);
extern void proc_fork_child_entry(void *arg) __attribute__((noreturn));
extern task_t *scheduler_add_forked_task(struct proc *p);
extern void microtime(struct timeval *tv);
extern void nanotime(struct timespec *ts);
extern void nanouptime(struct timespec *ts);

#define TIMESPEC_TO_NSEC(ts) \
	((uint64_t)(ts)->tv_sec * 1000000000ULL + (uint64_t)(ts)->tv_nsec)

static bool syscall_trace_enabled = false;

void
syscall_enable_trace(bool enable)
{
	syscall_trace_enabled = enable;
}

void
sys_exit(int status)
{
	task_t *current = scheduler_get_current_task();
	if (!current) {
		tty_printf("\nProcess exited with status: %d (no scheduler context)\n", status);
		asm volatile("cli; hlt");
		while (1);
	}

	struct proc *p = proc_get_current();
	if (!p || !p->p_p) {
		asm volatile("cli; hlt");
		while (1);
	}

	struct process *ps = p->p_p;
	struct limine_hhdm_response *hhdm = boot_get_hhdm();
	uint64_t hhdm_offset = hhdm ? hhdm->offset : 0;

	/* Free user space memory */
	if (ps->ps_vmspace) {
		uint64_t start = USER_SPACE_START;
		uint64_t end = ps->ps_brk;

		if (end > start) {
			uint64_t current_page = (start + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
			while (current_page < end) {
				uint64_t phys_addr = mmu_get_physical_address(ps->ps_vmspace, current_page);
				if (phys_addr != 0) {
					paging_unmap_range(ps->ps_vmspace, current_page, 1);
					void *page_virt = (void *)(phys_addr + hhdm_offset);
					pmm_free(page_virt);
				}
				current_page += PAGE_SIZE;
			}
		}
	}

	/* Close all file descriptors */
	uint64_t lock_flags;
	spinlock_acquire_irqsave(&ps->ps_lock, &lock_flags);

	for (int i = 0; i < MAX_OPEN_FILES; i++) {
		if (ps->ps_fd_table[i].file != NULL) {
			vfs_file_t *file = (vfs_file_t *)ps->ps_fd_table[i].file;
			ps->ps_fd_table[i].file = NULL;
			ps->ps_fd_table[i].flags = 0;
			
			/* Close outside lock */
			spinlock_release_irqrestore(&ps->ps_lock, lock_flags);
			vfs_close(file);
			spinlock_acquire_irqsave(&ps->ps_lock, &lock_flags);
		}
	}

	spinlock_release_irqrestore(&ps->ps_lock, lock_flags);

	/* Free VFS context */
	if (ps->ps_vfs_ctx) {
		vfs_context_free(ps->ps_vfs_ctx);
		ps->ps_vfs_ctx = NULL;
	}

	scheduler_exit_task(status);

	while (1) {
		asm volatile("hlt");
	}
}

int64_t
sys_fork(syscall_registers_t *regs)
{
	struct proc *parent_proc = proc_get_current();
	if (!parent_proc || !parent_proc->p_p)
		return -ESRCH;

	struct process *parent_ps = parent_proc->p_p;

	/* Allocate child process */
	struct process *child_ps = process_alloc(parent_ps->ps_comm);
	if (!child_ps)
		return -ENOMEM;

	/* Allocate child thread */
	struct proc *child_proc = proc_alloc(child_ps, parent_proc->p_name);
	if (!child_proc) {
		process_free(child_ps);
		return -ENOMEM;
	}

	/* Set up process relationships with locking */
	uint64_t lock_flags;
	spinlock_acquire_irqsave(&parent_ps->ps_lock, &lock_flags);
	child_ps->ps_pptr = parent_ps;
	LIST_INSERT_HEAD(&parent_ps->ps_children, child_ps, ps_sibling);
	spinlock_release_irqrestore(&parent_ps->ps_lock, lock_flags);

	/* Copy address space */
	struct limine_hhdm_response *hhdm = boot_get_hhdm();
	uint64_t hhdm_offset = hhdm ? hhdm->offset : 0;
	pml4e_t *parent_pml4 = parent_ps->ps_vmspace->pml4;
	uint64_t pages_copied = 0;

	for (int pml4_idx = 0; pml4_idx < 256; pml4_idx++) {
		if (!(parent_pml4[pml4_idx] & PAGE_PRESENT))
			continue;

		uint64_t pdpt_phys = parent_pml4[pml4_idx] & PAGE_ADDR_MASK;
		pdpte_t *pdpt = (pdpte_t *)mmu_phys_to_virt(pdpt_phys);

		for (int pdpt_idx = 0; pdpt_idx < 512; pdpt_idx++) {
			if (!(pdpt[pdpt_idx] & PAGE_PRESENT) || (pdpt[pdpt_idx] & PAGE_HUGE))
				continue;

			uint64_t pd_phys = pdpt[pdpt_idx] & PAGE_ADDR_MASK;
			pde_t *pd_table = (pde_t *)mmu_phys_to_virt(pd_phys);

			for (int pd_idx = 0; pd_idx < 512; pd_idx++) {
				if (!(pd_table[pd_idx] & PAGE_PRESENT) || (pd_table[pd_idx] & PAGE_HUGE))
					continue;

				uint64_t pt_phys = pd_table[pd_idx] & PAGE_ADDR_MASK;
				pte_t *pt = (pte_t *)mmu_phys_to_virt(pt_phys);

				for (int pt_idx = 0; pt_idx < 512; pt_idx++) {
					if (!(pt[pt_idx] & PAGE_PRESENT))
						continue;

					uint64_t src_phys = pt[pt_idx] & PAGE_ADDR_MASK;
					uint64_t flags = pt[pt_idx] & ~PAGE_ADDR_MASK;

					void *new_page = pmm_alloc();
					if (!new_page) {
						proc_free(child_proc);
						process_free(child_ps);
						return -ENOMEM;
					}

					void *src_page = (void *)(src_phys + hhdm_offset);
					memcpy(new_page, src_page, PAGE_SIZE);

					uint64_t new_phys = (uint64_t)new_page - hhdm_offset;
					uint64_t virt = ((uint64_t)pml4_idx << PML4_SHIFT) |
					                ((uint64_t)pdpt_idx << PDPT_SHIFT) |
					                ((uint64_t)pd_idx << PD_SHIFT) |
					                ((uint64_t)pt_idx << PT_SHIFT);

					if (!mmu_map_page(child_ps->ps_vmspace, virt, new_phys, flags)) {
						pmm_free(new_page);
						proc_free(child_proc);
						process_free(child_ps);
						return -ENOMEM;
					}

					pages_copied++;
				}
			}
		}
	}

	/* Copy brk and flags */
	spinlock_acquire_irqsave(&parent_ps->ps_lock, &lock_flags);
	child_ps->ps_brk = parent_ps->ps_brk;
	child_ps->ps_flags = parent_ps->ps_flags & ~PS_EMBRYO;

	/* Duplicate file descriptors */
	for (int i = 0; i < MAX_OPEN_FILES; i++) {
		child_ps->ps_fd_table[i] = parent_ps->ps_fd_table[i];
		if (child_ps->ps_fd_table[i].file) {
			vfs_file_t *file = (vfs_file_t *)child_ps->ps_fd_table[i].file;
			uint64_t file_flags;
			spinlock_acquire_irqsave(&file->f_lock, &file_flags);
			file->f_refcount++;
			spinlock_release_irqrestore(&file->f_lock, file_flags);
		}
	}

	/* Duplicate VFS context */
	if (parent_ps->ps_vfs_ctx) {
		child_ps->ps_vfs_ctx = vfs_context_dup(parent_ps->ps_vfs_ctx);
		if (!child_ps->ps_vfs_ctx) {
			spinlock_release_irqrestore(&parent_ps->ps_lock, lock_flags);
			proc_free(child_proc);
			process_free(child_ps);
			return -ENOMEM;
		}
	}

	spinlock_release_irqrestore(&parent_ps->ps_lock, lock_flags);

	/* Create trapframe for child */
	struct trapframe *child_tf = (struct trapframe *)pmm_alloc();
	if (!child_tf) {
		proc_free(child_proc);
		process_free(child_ps);
		return -ENOMEM;
	}

	memset(child_tf, 0, PAGE_SIZE);
	child_tf->tf_rax = 0;  /* Child returns 0 */
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

	child_proc->p_md.md_regs = child_tf;

	/* Set up kernel stack */
	uint64_t kstack_top = (uint64_t)child_proc->p_kstack + PROCESS_KERNEL_STACK_SIZE;
	kstack_top -= sizeof(cpu_state_t);
	kstack_top &= ~0xFULL;

	cpu_state_t *cpu_state = (cpu_state_t *)kstack_top;
	memset(cpu_state, 0, sizeof(cpu_state_t));
	cpu_state->rip = (uint64_t)proc_fork_child_entry;
	cpu_state->rdi = (uint64_t)child_proc;
	cpu_state->rsp = kstack_top;
	cpu_state->cr3 = child_ps->ps_vmspace->phys_addr;
	cpu_state->rflags = 0x202;
	cpu_state->cs = GDT_SELECTOR_KERNEL_CODE;
	cpu_state->ss = GDT_SELECTOR_KERNEL_DATA;

	child_proc->p_md.md_cpu_state = cpu_state;
	child_proc->p_kstack_top = kstack_top;
	child_proc->p_stat = SRUN;

	/* Add to scheduler */
	task_t *child_task = scheduler_add_forked_task(child_proc);
	if (!child_task) {
		pmm_free(child_tf);
		proc_free(child_proc);
		process_free(child_ps);
		return -ENOMEM;
	}

	return (int64_t)child_ps->ps_pid;
}

int64_t
sys_getpid(void)
{
	struct proc *p = proc_get_current();
	if (!p || !p->p_p)
		return -ESRCH;
	return (int64_t)p->p_p->ps_pid;
}

int64_t
sys_getppid(void)
{
	struct proc *p = proc_get_current();
	if (!p || !p->p_p)
		return -ESRCH;

	uint64_t lock_flags;
	spinlock_acquire_irqsave(&p->p_p->ps_lock, &lock_flags);
	struct process *parent = p->p_p->ps_pptr;
	pid_t ppid = parent ? parent->ps_pid : 0;
	spinlock_release_irqrestore(&p->p_p->ps_lock, lock_flags);

	return (int64_t)ppid;
}

int64_t
sys_read(int fd, void *buf, size_t count)
{
	if (count == 0)
		return 0;

	if (!is_user_range(buf, count))
		return -EFAULT;

	struct proc *p = proc_get_current();
	if (!p || !p->p_p)
		return -ESRCH;

	/* Special handling for stdin */
	if (fd == 0) {
		char *buffer = (char *)buf;
		size_t bytes_read = 0;
		bool got_newline = false;

		while (bytes_read < count && !got_newline) {
			while (!keyboard_has_key())
				asm volatile("sti; hlt");

			char c = keyboard_getchar();

			if (c == '\n' || c == '\r') {
				buffer[bytes_read++] = '\n';
				tty_putchar('\n');
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

			if (c == 3) {  /* Ctrl-C */
				tty_putchar('^');
				tty_putchar('C');
				tty_putchar('\n');
				return -EINTR;
			}

			if (c == 4) {  /* Ctrl-D (EOF) */
				if (bytes_read == 0)
					return 0;
				got_newline = true;
				break;
			}

			if (bytes_read < count) {
				buffer[bytes_read++] = c;
				tty_putchar(c);
			}
		}

		return bytes_read;
	}

	/* Get file with proper locking */
	vfs_file_t *file = fd_getfile(p->p_p, fd);
	if (!file)
		return -EBADF;

	/* Check file is readable */
	if ((file->f_flags & VFS_O_WRONLY) == VFS_O_WRONLY)
		return -EBADF;

	ssize_t result = vfs_read(file, buf, count);
	if (result < 0)
		return -vfs_errno((int)result);

	return result;
}

int64_t
sys_write(int fd, const void *buf, size_t count)
{
	if (count == 0)
		return 0;

	if (!is_user_range(buf, count))
		return -EFAULT;

	struct proc *p = proc_get_current();
	if (!p || !p->p_p)
		return -ESRCH;

	/* Special handling for stdout/stderr */
	if (fd == 1 || fd == 2) {
		const char *str = (const char *)buf;
		for (size_t i = 0; i < count; i++)
			tty_putchar(str[i]);
		return count;
	}

	/* Get file with proper locking */
	vfs_file_t *file = fd_getfile(p->p_p, fd);
	if (!file)
		return -EBADF;

	/* Check file is writable */
	if ((file->f_flags & VFS_O_RDONLY) == VFS_O_RDONLY)
		return -EBADF;

	ssize_t result = vfs_write(file, buf, count);
	if (result < 0)
		return -vfs_errno((int)result);

	return result;
}

int64_t
sys_open(const char *path, uint32_t flags, mode_t mode)
{
	struct proc *p = proc_get_current();
	if (!p || !p->p_p)
		return -ESRCH;

	/* Validate and copy path */
	ssize_t path_len = validate_user_string(path, VFS_MAX_PATH);
	if (path_len < 0)
		return path_len;

	if (path_len == 0)
		return -ENOENT;

	char kpath[VFS_MAX_PATH];
	int ret = copyinstr(path, kpath, sizeof(kpath), NULL);
	if (ret < 0)
		return ret;

	/* Open file */
	vfs_file_t *file;
	ret = vfs_open(kpath, flags, mode, &file);
	if (ret != VFS_SUCCESS)
		return -vfs_errno(ret);

	/* Allocate fd */
	int fd_flags = (flags & O_CLOEXEC) ? FD_CLOEXEC : 0;
	int fd = fd_alloc(p->p_p, 3, file, fd_flags);
	if (fd < 0) {
		vfs_close(file);
		return fd;
	}

	return fd;
}

int64_t
sys_close(int fd)
{
	struct proc *p = proc_get_current();
	if (!p || !p->p_p)
		return -ESRCH;

	return fd_close(p->p_p, fd);
}

int64_t
sys_creat(const char *path, mode_t mode)
{
	return sys_open(path, O_CREAT | O_WRONLY | O_TRUNC, mode);
}

int64_t
sys_openat(int dirfd, const char *pathname, uint32_t flags, mode_t mode)
{
	struct proc *p = proc_get_current();
	if (!p || !p->p_p)
		return -ESRCH;

	/* Validate and copy pathname */
	ssize_t path_len = validate_user_string(pathname, VFS_MAX_PATH);
	if (path_len < 0)
		return path_len;

	if (path_len == 0)
		return -EINVAL;

	char kpath[VFS_MAX_PATH];
	int ret = copyinstr(pathname, kpath, sizeof(kpath), NULL);
	if (ret < 0)
		return ret;

	/* If absolute path, ignore dirfd */
	if (kpath[0] == '/')
		return sys_open(kpath, flags, mode);

	/* Handle AT_FDCWD */
	if (dirfd == AT_FDCWD) {
		/* TODO: Proper cwd support */
		char abs_path[VFS_MAX_PATH];
		abs_path[0] = '/';
		size_t len = strlen(kpath);
		if (len + 2 > VFS_MAX_PATH)
			return -ENAMETOOLONG;
		memcpy(abs_path + 1, kpath, len + 1);
		return sys_open(abs_path, flags, mode);
	}

	/* Validate dirfd */
	vfs_file_t *dir_file = fd_getfile(p->p_p, dirfd);
	if (!dir_file)
		return -EBADF;

	vnode_t *dir_vnode = dir_file->f_vnode;
	if ((dir_vnode->v_mode & VFS_IFMT) != VFS_IFDIR)
		return -ENOTDIR;

	/* TODO: Implement proper relative path resolution */
	return -ENOTSUP;
}

off_t
sys_lseek(int fd, off_t offset, int whence)
{
	struct proc *p = proc_get_current();
	if (!p || !p->p_p)
		return -ESRCH;

	vfs_file_t *file = fd_getfile(p->p_p, fd);
	if (!file)
		return -EBADF;

	if (whence != VFS_SEEK_SET && whence != VFS_SEEK_CUR && whence != VFS_SEEK_END)
		return -EINVAL;

	off_t result = vfs_seek(file, offset, whence);
	if (result < 0)
		return -vfs_errno((int)result);

	return result;
}

int64_t
sys_dup(int oldfd)
{
	struct proc *p = proc_get_current();
	if (!p || !p->p_p)
		return -ESRCH;

	vfs_file_t *file = fd_getfile(p->p_p, oldfd);
	if (!file)
		return -EBADF;

	vfs_file_t *dup_file = vfs_file_dup(file);
	if (!dup_file)
		return -ENOMEM;

	int newfd = fd_alloc(p->p_p, 0, dup_file, 0);
	if (newfd < 0) {
		vfs_close(dup_file);
		return newfd;
	}

	return newfd;
}

int64_t
sys_dup2(int oldfd, int newfd)
{
	struct proc *p = proc_get_current();
	if (!p || !p->p_p)
		return -ESRCH;

	struct process *ps = p->p_p;

	if (oldfd < 0 || oldfd >= MAX_OPEN_FILES)
		return -EBADF;
	if (newfd < 0 || newfd >= MAX_OPEN_FILES)
		return -EBADF;

	if (oldfd == newfd) {
		vfs_file_t *file = fd_getfile(ps, oldfd);
		return file ? newfd : -EBADF;
	}

	vfs_file_t *oldfile = fd_getfile(ps, oldfd);
	if (!oldfile)
		return -EBADF;

	vfs_file_t *newfile = vfs_file_dup(oldfile);
	if (!newfile)
		return -ENOMEM;

	uint64_t lock_flags;
	spinlock_acquire_irqsave(&ps->ps_lock, &lock_flags);

	vfs_file_t *oldnew = ps->ps_fd_table[newfd].file;
	ps->ps_fd_table[newfd].file = newfile;
	ps->ps_fd_table[newfd].flags = 0;

	spinlock_release_irqrestore(&ps->ps_lock, lock_flags);

	if (oldnew)
		vfs_close(oldnew);

	return newfd;
}

int64_t
sys_fcntl(int fd, int cmd, uint64_t arg)
{
	struct proc *p = proc_get_current();
	if (!p || !p->p_p)
		return -ESRCH;

	struct process *ps = p->p_p;

	if (fd < 0 || fd >= MAX_OPEN_FILES)
		return -EBADF;

	switch (cmd) {
	case F_DUPFD:
	case F_DUPFD_CLOEXEC: {
		int minfd = (int)arg;
		if (minfd < 0 || minfd >= MAX_OPEN_FILES)
			return -EINVAL;

		vfs_file_t *file = fd_getfile(ps, fd);
		if (!file)
			return -EBADF;

		vfs_file_t *newfile = vfs_file_dup(file);
		if (!newfile)
			return -ENOMEM;

		int fd_flags = (cmd == F_DUPFD_CLOEXEC) ? FD_CLOEXEC : 0;
		int newfd = fd_alloc(ps, minfd, newfile, fd_flags);
		if (newfd < 0) {
			vfs_close(newfile);
			return newfd;
		}

		return newfd;
	}

	case F_GETFD:
		return fd_getflags(ps, fd);

	case F_SETFD:
		return fd_setflags(ps, fd, (int)arg & FD_CLOEXEC);

	case F_GETFL: {
		vfs_file_t *file = fd_getfile(ps, fd);
		if (!file)
			return -EBADF;

		int flags = 0;
		if ((file->f_flags & VFS_O_RDWR) == VFS_O_RDWR)
			flags |= O_RDWR;
		else if (file->f_flags & VFS_O_WRONLY)
			flags |= O_WRONLY;
		else
			flags |= O_RDONLY;

		if (file->f_flags & VFS_O_APPEND) flags |= O_APPEND;
		if (file->f_flags & VFS_O_CREAT) flags |= O_CREAT;
		if (file->f_flags & VFS_O_TRUNC) flags |= O_TRUNC;
		if (file->f_flags & VFS_O_EXCL) flags |= O_EXCL;

		return flags;
	}

	case F_SETFL: {
		vfs_file_t *file = fd_getfile(ps, fd);
		if (!file)
			return -EBADF;

		uint64_t lock_flags;
		spinlock_acquire_irqsave(&file->f_lock, &lock_flags);
		file->f_flags &= ~(VFS_O_APPEND);
		if (arg & O_APPEND)
			file->f_flags |= VFS_O_APPEND;
		spinlock_release_irqrestore(&file->f_lock, lock_flags);

		return 0;
	}

	case F_GETOWN:
	case F_SETOWN:
	case F_GETLK:
	case F_SETLK:
	case F_SETLKW:
		return -ENOSYS;

	default:
		return -EINVAL;
	}
}

int64_t
sys_mkdir(const char *path, mode_t mode)
{
	ssize_t path_len = validate_user_string(path, VFS_MAX_PATH);
	if (path_len < 0)
		return path_len;

	if (path_len == 0)
		return -EINVAL;

	char kpath[VFS_MAX_PATH];
	int ret = copyinstr(path, kpath, sizeof(kpath), NULL);
	if (ret < 0)
		return ret;

	ret = vfs_mkdir(kpath, mode);
	if (ret != VFS_SUCCESS)
		return -vfs_errno(ret);

	return 0;
}

int64_t
sys_rmdir(const char *path)
{
	ssize_t path_len = validate_user_string(path, VFS_MAX_PATH);
	if (path_len < 0)
		return path_len;

	if (path_len == 0)
		return -EINVAL;

	char kpath[VFS_MAX_PATH];
	int ret = copyinstr(path, kpath, sizeof(kpath), NULL);
	if (ret < 0)
		return ret;

	ret = vfs_rmdir(kpath);
	if (ret != VFS_SUCCESS)
		return -vfs_errno(ret);

	return 0;
}

int64_t
sys_getcwd(char *buf, size_t size)
{
	if (buf == NULL || size == 0)
		return -EINVAL;

	if (!is_user_range(buf, size))
		return -EFAULT;

	struct proc *p = proc_get_current();
	if (!p || !p->p_p)
		return -ESRCH;

	uint64_t lock_flags;
	spinlock_acquire_irqsave(&p->p_p->ps_lock, &lock_flags);
	vfs_context_t *ctx = p->p_p->ps_vfs_ctx;

	if (!ctx) {
		spinlock_release_irqrestore(&p->p_p->ps_lock, lock_flags);
		if (size < 2)
			return -ERANGE;
		char root[] = "/";
		return copyout(root, buf, 2) ? -EFAULT : (int64_t)buf;
	}

	spinlock_release_irqrestore(&p->p_p->ps_lock, lock_flags);

	char kbuf[VFS_MAX_PATH];
	int ret = vfs_getcwd(ctx, kbuf, sizeof(kbuf));
	if (ret != 0)
		return -vfs_errno(ret);

	size_t len = strlen(kbuf) + 1;
	if (len > size)
		return -ERANGE;

	ret = copyout(kbuf, buf, len);
	return ret ? ret : (int64_t)buf;
}

int64_t
sys_chdir(const char *path)
{
	ssize_t path_len = validate_user_string(path, VFS_MAX_PATH);
	if (path_len < 0)
		return path_len;

	if (path_len == 0)
		return -EINVAL;

	char kpath[VFS_MAX_PATH];
	int ret = copyinstr(path, kpath, sizeof(kpath), NULL);
	if (ret < 0)
		return ret;

	struct proc *p = proc_get_current();
	if (!p || !p->p_p)
		return -ESRCH;

	uint64_t lock_flags;
	spinlock_acquire_irqsave(&p->p_p->ps_lock, &lock_flags);

	vfs_context_t *ctx = p->p_p->ps_vfs_ctx;
	if (!ctx) {
		ctx = vfs_context_create();
		if (!ctx) {
			spinlock_release_irqrestore(&p->p_p->ps_lock, lock_flags);
			return -ENOMEM;
		}
		p->p_p->ps_vfs_ctx = ctx;
	}

	spinlock_release_irqrestore(&p->p_p->ps_lock, lock_flags);

	ret = vfs_chdir(ctx, kpath);
	if (ret != 0)
		return -vfs_errno(ret);

	return 0;
}

int64_t
sys_fchdir(int fd)
{
	struct proc *p = proc_get_current();
	if (!p || !p->p_p)
		return -ESRCH;

	vfs_file_t *file = fd_getfile(p->p_p, fd);
	if (!file)
		return -EBADF;

	vnode_t *vnode = file->f_vnode;
	if ((vnode->v_mode & VFS_IFMT) != VFS_IFDIR)
		return -ENOTDIR;

	/* TODO: Implement reverse path lookup */
	return -ENOTSUP;
}

int64_t
sys_getdents(int fd, void *dirp, size_t count)
{
	if (dirp == NULL || count == 0)
		return -EINVAL;

	if (!is_user_range(dirp, count))
		return -EFAULT;

	struct proc *p = proc_get_current();
	if (!p || !p->p_p)
		return -ESRCH;

	vfs_file_t *file = fd_getfile(p->p_p, fd);
	if (!file)
		return -EBADF;

	vnode_t *vnode = file->f_vnode;
	if ((vnode->v_mode & VFS_IFMT) != VFS_IFDIR)
		return -ENOTDIR;

	struct linux_dirent {
		unsigned long d_ino;
		unsigned long d_off;
		unsigned short d_reclen;
		char d_name[];
	};

	char *buf = (char *)dirp;
	size_t bytes_written = 0;

	while (bytes_written < count) {
		vfs_dirent_t vfs_dirent;
		int ret = vfs_readdir(file, &vfs_dirent);

		if (ret != 0) {
			if (bytes_written > 0)
				return bytes_written;
			return (ret == -ENOENT) ? 0 : -vfs_errno(ret);
		}

		size_t name_len = strlen(vfs_dirent.d_name);
		size_t reclen = sizeof(struct linux_dirent) + name_len + 1;
		reclen = (reclen + sizeof(long) - 1) & ~(sizeof(long) - 1);

		if (bytes_written + reclen > count)
			break;

		struct linux_dirent *dent = (struct linux_dirent *)(buf + bytes_written);
		dent->d_ino = vfs_dirent.d_ino;
		dent->d_off = vfs_dirent.d_off;
		dent->d_reclen = reclen;
		memcpy(dent->d_name, vfs_dirent.d_name, name_len + 1);

		char *type_ptr = (char *)dent + reclen - 1;
		*type_ptr = vfs_dirent.d_type;

		bytes_written += reclen;
	}

	return bytes_written;
}

int64_t
sys_unlink(const char *path)
{
	ssize_t path_len = validate_user_string(path, VFS_MAX_PATH);
	if (path_len < 0)
		return path_len;

	if (path_len == 0)
		return -EINVAL;

	char kpath[VFS_MAX_PATH];
	int ret = copyinstr(path, kpath, sizeof(kpath), NULL);
	if (ret < 0)
		return ret;

	/* Check if it's a directory */
	vnode_t *vnode;
	int lookup_ret = vfs_lookup(kpath, &vnode);
	if (lookup_ret == 0) {
		if ((vnode->v_mode & VFS_IFMT) == VFS_IFDIR) {
			vfs_vnode_unref(vnode);
			return -EISDIR;
		}
		vfs_vnode_unref(vnode);
	}

	ret = vfs_unlink(kpath);
	if (ret != VFS_SUCCESS)
		return -vfs_errno(ret);

	return 0;
}

int64_t
sys_link(const char *oldpath, const char *newpath)
{
	ssize_t old_len = validate_user_string(oldpath, VFS_MAX_PATH);
	if (old_len < 0)
		return old_len;

	ssize_t new_len = validate_user_string(newpath, VFS_MAX_PATH);
	if (new_len < 0)
		return new_len;

	if (old_len == 0 || new_len == 0)
		return -EINVAL;

	char kold[VFS_MAX_PATH], knew[VFS_MAX_PATH];
	int ret = copyinstr(oldpath, kold, sizeof(kold), NULL);
	if (ret < 0)
		return ret;

	ret = copyinstr(newpath, knew, sizeof(knew), NULL);
	if (ret < 0)
		return ret;

	ret = vfs_link(kold, knew);
	if (ret != 0)
		return -vfs_errno(ret);

	return 0;
}

int64_t
sys_symlink(const char *target, const char *linkpath)
{
	ssize_t target_len = validate_user_string(target, VFS_MAX_PATH);
	if (target_len < 0)
		return target_len;

	ssize_t link_len = validate_user_string(linkpath, VFS_MAX_PATH);
	if (link_len < 0)
		return link_len;

	if (target_len == 0 || link_len == 0)
		return -EINVAL;

	char ktarget[VFS_MAX_PATH], klink[VFS_MAX_PATH];
	int ret = copyinstr(target, ktarget, sizeof(ktarget), NULL);
	if (ret < 0)
		return ret;

	ret = copyinstr(linkpath, klink, sizeof(klink), NULL);
	if (ret < 0)
		return ret;

	ret = vfs_symlink(ktarget, klink);
	if (ret != 0)
		return -vfs_errno(ret);

	return 0;
}

int64_t
sys_readlink(const char *path, char *buf, size_t bufsiz)
{
	if (buf == NULL || bufsiz == 0)
		return -EINVAL;

	ssize_t path_len = validate_user_string(path, VFS_MAX_PATH);
	if (path_len < 0)
		return path_len;

	if (path_len == 0)
		return -EINVAL;

	if (!is_user_range(buf, bufsiz))
		return -EFAULT;

	char kpath[VFS_MAX_PATH];
	int ret = copyinstr(path, kpath, sizeof(kpath), NULL);
	if (ret < 0)
		return ret;

	char kbuf[VFS_MAX_PATH];
	ret = vfs_readlink(kpath, kbuf, sizeof(kbuf));
	if (ret < 0)
		return -vfs_errno(ret);

	size_t len = (size_t)ret;
	if (len > bufsiz)
		len = bufsiz;

	ret = copyout(kbuf, buf, len);
	return ret ? ret : (int64_t)len;
}

int64_t
sys_rename(const char *oldpath, const char *newpath)
{
	ssize_t old_len = validate_user_string(oldpath, VFS_MAX_PATH);
	if (old_len < 0)
		return old_len;

	ssize_t new_len = validate_user_string(newpath, VFS_MAX_PATH);
	if (new_len < 0)
		return new_len;

	if (old_len == 0 || new_len == 0)
		return -EINVAL;

	char kold[VFS_MAX_PATH], knew[VFS_MAX_PATH];
	int ret = copyinstr(oldpath, kold, sizeof(kold), NULL);
	if (ret < 0)
		return ret;

	ret = copyinstr(newpath, knew, sizeof(knew), NULL);
	if (ret < 0)
		return ret;

	ret = vfs_rename(kold, knew);
	if (ret != 0)
		return -vfs_errno(ret);

	return 0;
}

int64_t
sys_truncate(const char *path, off_t length)
{
	if (length < 0)
		return -EINVAL;

	ssize_t path_len = validate_user_string(path, VFS_MAX_PATH);
	if (path_len < 0)
		return path_len;

	if (path_len == 0)
		return -EINVAL;

	char kpath[VFS_MAX_PATH];
	int ret = copyinstr(path, kpath, sizeof(kpath), NULL);
	if (ret < 0)
		return ret;

	ret = vfs_truncate(kpath, length);
	if (ret != 0)
		return -vfs_errno(ret);

	return 0;
}

int64_t
sys_ftruncate(int fd, off_t length)
{
	if (length < 0)
		return -EINVAL;

	struct proc *p = proc_get_current();
	if (!p || !p->p_p)
		return -ESRCH;

	vfs_file_t *file = fd_getfile(p->p_p, fd);
	if (!file)
		return -EBADF;

	if (!(file->f_flags & (VFS_O_WRONLY | VFS_O_RDWR)))
		return -EINVAL;

	vnode_t *vnode = file->f_vnode;
	if (!vnode->v_ops || !vnode->v_ops->truncate)
		return -EINVAL;

	int ret = vnode->v_ops->truncate(vnode, length);
	if (ret != 0)
		return -vfs_errno(ret);

	return 0;
}

int64_t
sys_mknod(const char *path, mode_t mode, dev_t dev)
{
	ssize_t path_len = validate_user_string(path, VFS_MAX_PATH);
	if (path_len < 0)
		return path_len;

	if (path_len == 0)
		return -EINVAL;

	char kpath[VFS_MAX_PATH];
	int ret = copyinstr(path, kpath, sizeof(kpath), NULL);
	if (ret < 0)
		return ret;

	ret = vfs_mknod(kpath, mode, dev);
	if (ret != VFS_SUCCESS)
		return -vfs_errno(ret);

	return 0;
}

int64_t
sys_stat(const char *path, struct stat *statbuf)
{
	ssize_t path_len = validate_user_string(path, VFS_MAX_PATH);
	if (path_len < 0)
		return path_len;

	if (path_len == 0)
		return -ENOENT;

	if (!is_user_range(statbuf, sizeof(struct stat)))
		return -EFAULT;

	char kpath[VFS_MAX_PATH];
	int ret = copyinstr(path, kpath, sizeof(kpath), NULL);
	if (ret < 0)
		return ret;

	vfs_stat_t kstat;
	memset(&kstat, 0, sizeof(vfs_stat_t));

	ret = vfs_stat(kpath, &kstat);
	if (ret != VFS_SUCCESS)
		return -vfs_errno(ret);

	struct stat ustat;
	ustat.st_dev = kstat.st_dev;
	ustat.st_ino = kstat.st_ino;
	ustat.st_mode = kstat.st_mode;
	ustat.st_nlink = kstat.st_nlink;
	ustat.st_uid = kstat.st_uid;
	ustat.st_gid = kstat.st_gid;
	ustat.st_rdev = kstat.st_rdev;
	ustat.st_size = kstat.st_size;
	ustat.st_blksize = kstat.st_blksize;
	ustat.st_blocks = kstat.st_blocks;
	ustat.st_atime = kstat.st_atime;
	ustat.st_mtime = kstat.st_mtime;
	ustat.st_ctime = kstat.st_ctime;

	return copyout(&ustat, statbuf, sizeof(struct stat));
}

int64_t
sys_fstat(int fd, struct stat *statbuf)
{
	if (!is_user_range(statbuf, sizeof(struct stat)))
		return -EFAULT;

	struct proc *p = proc_get_current();
	if (!p || !p->p_p)
		return -ESRCH;

	vfs_file_t *file = fd_getfile(p->p_p, fd);
	if (!file)
		return -EBADF;

	vfs_stat_t kstat;
	memset(&kstat, 0, sizeof(vfs_stat_t));

	int ret = vfs_fstat(file, &kstat);
	if (ret != VFS_SUCCESS)
		return -vfs_errno(ret);

	struct stat ustat;
	ustat.st_dev = kstat.st_dev;
	ustat.st_ino = kstat.st_ino;
	ustat.st_mode = kstat.st_mode;
	ustat.st_nlink = kstat.st_nlink;
	ustat.st_uid = kstat.st_uid;
	ustat.st_gid = kstat.st_gid;
	ustat.st_rdev = kstat.st_rdev;
	ustat.st_size = kstat.st_size;
	ustat.st_blksize = kstat.st_blksize;
	ustat.st_blocks = kstat.st_blocks;
	ustat.st_atime = kstat.st_atime;
	ustat.st_mtime = kstat.st_mtime;
	ustat.st_ctime = kstat.st_ctime;

	return copyout(&ustat, statbuf, sizeof(struct stat));
}

int64_t
sys_lstat(const char *path, struct stat *statbuf)
{
	ssize_t path_len = validate_user_string(path, VFS_MAX_PATH);
	if (path_len < 0)
		return path_len;

	if (path_len == 0)
		return -ENOENT;

	if (!is_user_range(statbuf, sizeof(struct stat)))
		return -EFAULT;

	char kpath[VFS_MAX_PATH];
	int ret = copyinstr(path, kpath, sizeof(kpath), NULL);
	if (ret < 0)
		return ret;

	vfs_stat_t kstat;
	memset(&kstat, 0, sizeof(vfs_stat_t));

	ret = vfs_lstat(kpath, &kstat);
	if (ret != VFS_SUCCESS)
		return -vfs_errno(ret);

	struct stat ustat;
	ustat.st_dev = kstat.st_dev;
	ustat.st_ino = kstat.st_ino;
	ustat.st_mode = kstat.st_mode;
	ustat.st_nlink = kstat.st_nlink;
	ustat.st_uid = kstat.st_uid;
	ustat.st_gid = kstat.st_gid;
	ustat.st_rdev = kstat.st_rdev;
	ustat.st_size = kstat.st_size;
	ustat.st_blksize = kstat.st_blksize;
	ustat.st_blocks = kstat.st_blocks;
	ustat.st_atime = kstat.st_atime;
	ustat.st_mtime = kstat.st_mtime;
	ustat.st_ctime = kstat.st_ctime;

	return copyout(&ustat, statbuf, sizeof(struct stat));
}

int64_t
sys_access(const char *path, int mode)
{
	ssize_t path_len = validate_user_string(path, VFS_MAX_PATH);
	if (path_len < 0)
		return path_len;

	if (path_len == 0)
		return -EINVAL;

	char kpath[VFS_MAX_PATH];
	int ret = copyinstr(path, kpath, sizeof(kpath), NULL);
	if (ret < 0)
		return ret;

	ret = vfs_access(kpath, mode);
	if (ret != 0)
		return -vfs_errno(ret);

	return 0;
}

int64_t
sys_chmod(const char *path, mode_t mode)
{
	ssize_t path_len = validate_user_string(path, VFS_MAX_PATH);
	if (path_len < 0)
		return path_len;

	if (path_len == 0)
		return -EINVAL;

	char kpath[VFS_MAX_PATH];
	int ret = copyinstr(path, kpath, sizeof(kpath), NULL);
	if (ret < 0)
		return ret;

	ret = vfs_chmod(kpath, mode);
	if (ret != VFS_SUCCESS)
		return -vfs_errno(ret);

	return 0;
}

int64_t
sys_chown(const char *path, uid_t owner, gid_t group)
{
	ssize_t path_len = validate_user_string(path, VFS_MAX_PATH);
	if (path_len < 0)
		return path_len;

	if (path_len == 0)
		return -EINVAL;

	char kpath[VFS_MAX_PATH];
	int ret = copyinstr(path, kpath, sizeof(kpath), NULL);
	if (ret < 0)
		return ret;

	ret = vfs_chown(kpath, owner, group);
	if (ret != 0)
		return -vfs_errno(ret);

	return 0;
}

void *
sys_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
	struct proc *p = proc_get_current();
	if (!p || !p->p_p || !p->p_p->ps_vmspace)
		return (void *)(intptr_t)-EINVAL;

	struct process *ps = p->p_p;

	if (length == 0)
		return (void *)(intptr_t)-EINVAL;

	if (!(flags & MAP_ANONYMOUS))
		return (void *)(intptr_t)-EINVAL;

	if (!(flags & MAP_SHARED) && !(flags & MAP_PRIVATE))
		return (void *)(intptr_t)-EINVAL;

	size_t aligned_length = (length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	size_t num_pages = aligned_length / PAGE_SIZE;
	uint64_t virt_addr;

	if (flags & MAP_FIXED) {
		if (!is_user_address(addr))
			return (void *)(intptr_t)-EINVAL;
		virt_addr = (uint64_t)addr & ~(PAGE_SIZE - 1);
	} else {
		uint64_t lock_flags;
		spinlock_acquire_irqsave(&ps->ps_lock, &lock_flags);
		virt_addr = (ps->ps_brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
		spinlock_release_irqrestore(&ps->ps_lock, lock_flags);

		if (addr != NULL && is_user_address(addr)) {
			uint64_t hint = (uint64_t)addr & ~(PAGE_SIZE - 1);
			if (hint >= virt_addr)
				virt_addr = hint;
		}
	}

	uint64_t page_flags = PAGE_PRESENT | PAGE_USER;
	if (prot & PROT_WRITE)
		page_flags |= PAGE_WRITE;
	if (!(prot & PROT_EXEC))
		page_flags |= PAGE_NX;

	struct limine_hhdm_response *hhdm = boot_get_hhdm();
	uint64_t hhdm_offset = hhdm ? hhdm->offset : 0;

	for (size_t i = 0; i < num_pages; i++) {
		uint64_t virt_page = virt_addr + (i * PAGE_SIZE);

		void *page_virt = pmm_alloc();
		if (!page_virt) {
			/* Cleanup */
			for (size_t j = 0; j < i; j++) {
				uint64_t cleanup_virt = virt_addr + (j * PAGE_SIZE);
				uint64_t phys = mmu_get_physical_address(ps->ps_vmspace, cleanup_virt);
				if (phys != 0) {
					paging_unmap_range(ps->ps_vmspace, cleanup_virt, 1);
					pmm_free((void *)(phys + hhdm_offset));
				}
			}
			return (void *)(intptr_t)-ENOMEM;
		}

		memset(page_virt, 0, PAGE_SIZE);
		uint64_t phys_page = (uint64_t)page_virt - hhdm_offset;

		if (!paging_map_range(ps->ps_vmspace, virt_page, phys_page, 1, page_flags)) {
			pmm_free(page_virt);
			/* Cleanup */
			for (size_t j = 0; j < i; j++) {
				uint64_t cleanup_virt = virt_addr + (j * PAGE_SIZE);
				uint64_t phys = mmu_get_physical_address(ps->ps_vmspace, cleanup_virt);
				if (phys != 0) {
					paging_unmap_range(ps->ps_vmspace, cleanup_virt, 1);
					pmm_free((void *)(phys + hhdm_offset));
				}
			}
			return (void *)(intptr_t)-ENOMEM;
		}
	}

	uint64_t lock_flags;
	spinlock_acquire_irqsave(&ps->ps_lock, &lock_flags);
	if (virt_addr + aligned_length > ps->ps_brk)
		ps->ps_brk = virt_addr + aligned_length;
	spinlock_release_irqrestore(&ps->ps_lock, lock_flags);

	return (void *)virt_addr;
}

int64_t
sys_munmap(void *addr, size_t length)
{
	struct proc *p = proc_get_current();
	if (!p || !p->p_p || !p->p_p->ps_vmspace)
		return -ESRCH;

	if (!is_user_address(addr) || length == 0)
		return -EINVAL;

	struct process *ps = p->p_p;
	uint64_t virt_addr = (uint64_t)addr & ~(PAGE_SIZE - 1);
	size_t aligned_length = (length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	size_t num_pages = aligned_length / PAGE_SIZE;

	struct limine_hhdm_response *hhdm = boot_get_hhdm();
	uint64_t hhdm_offset = hhdm ? hhdm->offset : 0;

	for (size_t i = 0; i < num_pages; i++) {
		uint64_t virt_page = virt_addr + (i * PAGE_SIZE);
		uint64_t phys_addr = mmu_get_physical_address(ps->ps_vmspace, virt_page);

		paging_unmap_range(ps->ps_vmspace, virt_page, 1);

		if (phys_addr != 0) {
			void *page_virt = (void *)(phys_addr + hhdm_offset);
			pmm_free(page_virt);
		}
	}

	return 0;
}

int64_t
sys_mprotect(void *addr, size_t len, int prot)
{
	struct proc *p = proc_get_current();
	if (!p || !p->p_p || !p->p_p->ps_vmspace)
		return -ESRCH;

	if (!is_user_address(addr) || len == 0)
		return -EINVAL;

	uint64_t virt_addr = (uint64_t)addr & ~(PAGE_SIZE - 1);
	size_t aligned_length = (len + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	size_t num_pages = aligned_length / PAGE_SIZE;

	uint64_t page_flags = PAGE_PRESENT | PAGE_USER;
	if (prot & PROT_WRITE)
		page_flags |= PAGE_WRITE;
	if (!(prot & PROT_EXEC))
		page_flags |= PAGE_NX;

	if (!paging_protect_range(p->p_p->ps_vmspace, virt_addr, num_pages, page_flags))
		return -ENOMEM;

	return 0;
}

int64_t
sys_brk(void *addr)
{
	struct proc *p = proc_get_current();
	if (!p || !p->p_p || !p->p_p->ps_vmspace)
		return -ESRCH;

	struct process *ps = p->p_p;

	uint64_t lock_flags;
	spinlock_acquire_irqsave(&ps->ps_lock, &lock_flags);

	if (addr == NULL || addr == 0) {
		uint64_t brk = ps->ps_brk;
		spinlock_release_irqrestore(&ps->ps_lock, lock_flags);
		return (int64_t)brk;
	}

	uint64_t new_brk = (uint64_t)addr;

	if (!is_user_address(addr) || new_brk < USER_CODE_BASE ||
	    new_brk >= (USER_STACK_TOP - PROCESS_USER_STACK_SIZE)) {
		spinlock_release_irqrestore(&ps->ps_lock, lock_flags);
		return -ENOMEM;
	}

	uint64_t old_brk = ps->ps_brk;
	if (old_brk == 0) {
		old_brk = (USER_CODE_BASE + 0x100000) & ~(PAGE_SIZE - 1);
		ps->ps_brk = old_brk;
	}

	if (new_brk == old_brk) {
		spinlock_release_irqrestore(&ps->ps_lock, lock_flags);
		return (int64_t)old_brk;
	}

	spinlock_release_irqrestore(&ps->ps_lock, lock_flags);

	struct limine_hhdm_response *hhdm = boot_get_hhdm();
	uint64_t hhdm_offset = hhdm ? hhdm->offset : 0;

	if (new_brk > old_brk) {
		/* Expand */
		uint64_t old_page = (old_brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
		uint64_t new_page = (new_brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

		uint64_t current_page = old_page;
		while (current_page < new_page) {
			uint64_t phys_addr = mmu_get_physical_address(ps->ps_vmspace, current_page);

			if (phys_addr == 0) {
				void *page_virt = pmm_alloc();
				if (!page_virt)
					return -ENOMEM;

				memset(page_virt, 0, PAGE_SIZE);
				uint64_t phys_page = (uint64_t)page_virt - hhdm_offset;

				if (!paging_map_range(ps->ps_vmspace, current_page, phys_page, 1,
				                      PAGE_PRESENT | PAGE_WRITE | PAGE_USER)) {
					pmm_free(page_virt);
					return -ENOMEM;
				}
			}

			current_page += PAGE_SIZE;
		}
	} else {
		/* Shrink */
		uint64_t new_page = (new_brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
		uint64_t old_page = (old_brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

		uint64_t current_page = new_page;
		while (current_page < old_page) {
			uint64_t phys_addr = mmu_get_physical_address(ps->ps_vmspace, current_page);

			if (phys_addr != 0) {
				paging_unmap_range(ps->ps_vmspace, current_page, 1);
				void *page_virt = (void *)(phys_addr + hhdm_offset);
				pmm_free(page_virt);
			}

			current_page += PAGE_SIZE;
		}
	}

	spinlock_acquire_irqsave(&ps->ps_lock, &lock_flags);
	ps->ps_brk = new_brk;
	spinlock_release_irqrestore(&ps->ps_lock, lock_flags);

	return (int64_t)new_brk;
}

int64_t
sys_gethostname(char *name, size_t len)
{
	if (len == 0)
		return -EINVAL;

	if (!is_user_range(name, len))
		return -EFAULT;

	char kname[256];
	int ret = gethostname(kname, sizeof(kname));
	if (ret < 0)
		return ret;

	size_t copy_len = strlen(kname) + 1;
	if (copy_len > len)
		copy_len = len;

	return copyout(kname, name, copy_len);
}

int64_t
sys_gethostid(void)
{
	return gethostid();
}

int64_t
sys_sysinfo(struct sysinfo *info)
{
	if (!is_user_range(info, sizeof(struct sysinfo)))
		return -EFAULT;

	struct sysinfo kinfo;
	int ret = kern_sysinfo(&kinfo);
	if (ret < 0)
		return ret;

	return copyout(&kinfo, info, sizeof(struct sysinfo));
}

int64_t
sys_uname(struct utsname *buf)
{
	if (!is_user_range(buf, sizeof(struct utsname)))
		return -EFAULT;

	struct utsname kbuf;
	extern int kern_uname(struct utsname *);
	int ret = kern_uname(&kbuf);
	if (ret < 0)
		return ret;

	return copyout(&kbuf, buf, sizeof(struct utsname));
}

int64_t
sys_gettimeofday(struct timeval *tv, struct timezone *tz)
{
	if (tv != NULL) {
		if (!is_user_range(tv, sizeof(struct timeval)))
			return -EFAULT;

		struct timeval ktv;
		microtime(&ktv);

		if (copyout(&ktv, tv, sizeof(struct timeval)) < 0)
			return -EFAULT;
	}

	if (tz != NULL) {
		if (!is_user_range(tz, sizeof(struct timezone)))
			return -EFAULT;

		struct timezone ktz;
		ktz.tz_minuteswest = 0;
		ktz.tz_dsttime = 0;

		if (copyout(&ktz, tz, sizeof(struct timezone)) < 0)
			return -EFAULT;
	}

	return 0;
}

int64_t
sys_clock_gettime(clockid_t clock_id, struct timespec *tp)
{
	if (tp == NULL)
		return -EINVAL;

	if (!is_user_range(tp, sizeof(struct timespec)))
		return -EFAULT;

	struct timespec kts;

	switch (clock_id) {
	case 0:  /* CLOCK_REALTIME */
		nanotime(&kts);
		break;

	case 1:  /* CLOCK_MONOTONIC */
	case 4:  /* CLOCK_MONOTONIC_RAW */
	case 7:  /* CLOCK_BOOTTIME */
		nanouptime(&kts);
		break;

	default:
		return -EINVAL;
	}

	return copyout(&kts, tp, sizeof(struct timespec));
}

int64_t
sys_clock_getres(clockid_t clock_id, struct timespec *res)
{
	if (res == NULL)
		return -EINVAL;

	if (!is_user_range(res, sizeof(struct timespec)))
		return -EFAULT;

	struct timespec kres;

	switch (clock_id) {
	case 0:  /* CLOCK_REALTIME */
	case 1:  /* CLOCK_MONOTONIC */
	case 4:  /* CLOCK_MONOTONIC_RAW */
	case 7:  /* CLOCK_BOOTTIME */
		kres.tv_sec = 0;
		kres.tv_nsec = 1;
		break;

	default:
		return -EINVAL;
	}

	return copyout(&kres, res, sizeof(struct timespec));
}

int64_t
sys_nanosleep(const struct timespec *req, struct timespec *rem)
{
	if (req == NULL)
		return -EINVAL;

	if (!is_user_range(req, sizeof(struct timespec)))
		return -EFAULT;

	if (rem != NULL && !is_user_range(rem, sizeof(struct timespec)))
		return -EFAULT;

	struct timespec kreq;
	if (copyin(req, &kreq, sizeof(struct timespec)) < 0)
		return -EFAULT;

	if (!timespecisvalid(&kreq))
		return -EINVAL;

	uint64_t sleep_ns = TIMESPEC_TO_NSEC(&kreq);

	extern void tsc_delay_ns(uint64_t);
	tsc_delay_ns(sleep_ns);

	if (rem != NULL) {
		struct timespec krem = { 0, 0 };
		copyout(&krem, rem, sizeof(struct timespec));
	}

	return 0;
}

void
syscall_handler(syscall_registers_t *regs)
{
	uint64_t syscall_num = regs->rax;
	int64_t ret = -ENOSYS;

	if (syscall_trace_enabled) {
		tty_printf("[SYSCALL] %llu\n", syscall_num);
	}

	switch (syscall_num) {
	case SYSCALL_EXIT:
		sys_exit((int)regs->rdi);
		break;

	case SYSCALL_FORK:
		ret = sys_fork(regs);
		break;

	case SYSCALL_READ:
		ret = sys_read((int)regs->rdi, (void *)regs->rsi, (size_t)regs->rdx);
		break;

	case SYSCALL_WRITE:
		ret = sys_write((int)regs->rdi, (const void *)regs->rsi, (size_t)regs->rdx);
		break;

	case SYSCALL_OPEN:
		ret = sys_open((const char *)regs->rdi, (uint32_t)regs->rsi, (mode_t)regs->rdx);
		break;

	case SYSCALL_CLOSE:
		ret = sys_close((int)regs->rdi);
		break;

	case SYSCALL_CREAT:
		ret = sys_creat((const char *)regs->rdi, (mode_t)regs->rsi);
		break;

	case SYSCALL_OPENAT:
		ret = sys_openat((int)regs->rdi, (const char *)regs->rsi,
		                 (uint32_t)regs->rdx, (mode_t)regs->r10);
		break;

	case SYSCALL_MKDIR:
		ret = sys_mkdir((const char *)regs->rdi, (mode_t)regs->rsi);
		break;

	case SYSCALL_MKNOD:
		ret = sys_mknod((const char *)regs->rdi, (mode_t)regs->rsi, (dev_t)regs->rdx);
		break;

	case SYSCALL_RMDIR:
		ret = sys_rmdir((const char *)regs->rdi);
		break;

	case SYSCALL_UNLINK:
		ret = sys_unlink((const char *)regs->rdi);
		break;

	case SYSCALL_GETCWD:
		ret = sys_getcwd((char *)regs->rdi, (size_t)regs->rsi);
		break;

	case SYSCALL_CHDIR:
		ret = sys_chdir((const char *)regs->rdi);
		break;

	case SYSCALL_FCHDIR:
		ret = sys_fchdir((int)regs->rdi);
		break;

	case SYSCALL_GETDENTS:
	case SYSCALL_GETDENTS64:
		ret = sys_getdents((int)regs->rdi, (void *)regs->rsi, (size_t)regs->rdx);
		break;

	case SYSCALL_SYMLINK:
		ret = sys_symlink((const char *)regs->rdi, (const char *)regs->rsi);
		break;

	case SYSCALL_READLINK:
		ret = sys_readlink((const char *)regs->rdi, (char *)regs->rsi, (size_t)regs->rdx);
		break;

	case SYSCALL_LINK:
		ret = sys_link((const char *)regs->rdi, (const char *)regs->rsi);
		break;

	case SYSCALL_RENAME:
		ret = sys_rename((const char *)regs->rdi, (const char *)regs->rsi);
		break;

	case SYSCALL_TRUNCATE:
		ret = sys_truncate((const char *)regs->rdi, (off_t)regs->rsi);
		break;

	case SYSCALL_FTRUNCATE:
		ret = sys_ftruncate((int)regs->rdi, (off_t)regs->rsi);
		break;

	case SYSCALL_ACCESS:
		ret = sys_access((const char *)regs->rdi, (int)regs->rsi);
		break;

	case SYSCALL_CHOWN:
		ret = sys_chown((const char *)regs->rdi, (uid_t)regs->rsi, (gid_t)regs->rdx);
		break;

	case SYSCALL_CHMOD:
		ret = sys_chmod((const char *)regs->rdi, (mode_t)regs->rsi);
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
		ret = sys_stat((const char *)regs->rdi, (struct stat *)regs->rsi);
		break;

	case SYSCALL_FSTAT:
		ret = sys_fstat((int)regs->rdi, (struct stat *)regs->rsi);
		break;

	case SYSCALL_LSTAT:
		ret = sys_lstat((const char *)regs->rdi, (struct stat *)regs->rsi);
		break;

	case SYSCALL_LSEEK:
		ret = sys_lseek((int)regs->rdi, (off_t)regs->rsi, (int)regs->rdx);
		break;

	case SYSCALL_GETPID:
		ret = sys_getpid();
		break;

	case SYSCALL_GETPPID:
		ret = sys_getppid();
		break;

	case SYSCALL_MMAP: {
		void *result = sys_mmap((void *)regs->rdi, (size_t)regs->rsi,
		                        (int)regs->rdx, (int)regs->r10,
		                        (int)regs->r8, (off_t)regs->r9);
		ret = (int64_t)result;
		break;
	}

	case SYSCALL_MUNMAP:
		ret = sys_munmap((void *)regs->rdi, (size_t)regs->rsi);
		break;

	case SYSCALL_MPROTECT:
		ret = sys_mprotect((void *)regs->rdi, (size_t)regs->rsi, (int)regs->rdx);
		break;

	case SYSCALL_BRK:
		ret = sys_brk((void *)regs->rdi);
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

	case SYSCALL_GETTIMEOFDAY:
		ret = sys_gettimeofday((struct timeval *)regs->rdi,
		                       (struct timezone *)regs->rsi);
		break;

	case SYSCALL_CLOCK_GETTIME:
		ret = sys_clock_gettime((clockid_t)regs->rdi,
		                        (struct timespec *)regs->rsi);
		break;

	case SYSCALL_CLOCK_GETRES:
		ret = sys_clock_getres((clockid_t)regs->rdi,
		                       (struct timespec *)regs->rsi);
		break;

	case SYSCALL_NANOSLEEP:
		ret = sys_nanosleep((const struct timespec *)regs->rdi,
		                    (struct timespec *)regs->rsi);
		break;

	default:
		tty_printf("[SYSCALL] Unknown syscall: %llu\n", syscall_num);
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
	tty_printf("[SYSCALL] Handler initialized on INT 0x80\n");
}

void
syscall_print_stats(void)
{
	tty_printf("[SYSCALL] Statistics feature not yet implemented\n");
}
