#include <stdbool.h>
#include <syscall.h>
#include <scheduler.h>
#include <vfs.h>
#include <idt.h>
#include <gdt.h>
#include <sys/sysinfo.h>
#include <sys/errno.h>
#include <sys/mman.h>
#include <vmm.h>
#include <pmm.h>
#include <proc.h>
#include <string.h>

extern bool keyboard_has_key(void);
extern char keyboard_getchar(void);
extern void tty_printf(const char *fmt, ...);
extern void tty_putchar(char c);
extern void syscall_stub(void);

extern int kern_sysinfo(struct sysinfo *info);
extern int gethostname(char *name, size_t len);
extern int gethostid(void);

#define USER_SPACE_START 0x0000000000400000UL
#define USER_SPACE_END   0x00007FFFFFFFFFFFUL

static inline bool is_user_address(const void *addr) {
    uint64_t ptr = (uint64_t)addr;
    return ptr >= USER_SPACE_START && ptr < USER_SPACE_END;
}

static inline bool is_user_range(const void *addr, size_t len) {
    if (len == 0) return true;
    uint64_t start = (uint64_t)addr;
    uint64_t end = start + len;
    
    if (end < start) return false;
    
    return is_user_address(addr) && end <= USER_SPACE_END;
}

static uint64_t mmap_prot_to_page_flags(int prot, bool is_user) {
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

static uint64_t find_free_region(vmm_address_space_t *space, size_t size) {
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
            
            if (!(region_end <= region->virt_start || current >= region->virt_end)) {
                region_free = false;
                current = (region->virt_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
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

static inline int vfs_errno(int vfs_ret) {
    if (vfs_ret == VFS_SUCCESS) return 0;
    if (vfs_ret < 0) return -vfs_ret;
    return EIO;
}

void sys_exit(int status) {
    task_t *current = scheduler_get_current_task();
    
    if (current) {
        struct proc *p = proc_get_current();
        
        if (p != NULL && p->p_p != NULL && p->p_p->ps_vmspace != NULL) {
            struct process *ps = p->p_p;
            
            tty_printf("[SYSCALL] Cleaning up memory mappings for task %d\n", current->tid);
            
            extern struct limine_hhdm_response *boot_get_hhdm(void);
            struct limine_hhdm_response *hhdm = boot_get_hhdm();
            uint64_t hhdm_offset = hhdm ? hhdm->offset : 0;
            
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
                
                tty_printf("[SYSCALL] Unmapped memory range 0x%lx - 0x%lx\n", start, end);
            }
        }
        
        for (int i = 0; i < MAX_OPEN_FILES; i++) {
            if (current->fd_table[i] != NULL) {
                vfs_file_t *file = (vfs_file_t *)current->fd_table[i];
                vfs_close(file);
                current->fd_table[i] = NULL;
            }
        }
        
        tty_printf("[SYSCALL] Task %d (%s) exiting with status %d\n", 
                   current->tid, current->name, status);
        
        scheduler_exit_task(status);
        
        while(1) {
            asm volatile("hlt");
        }
    } else {
        tty_printf("\nProcess exited with status: %d (no scheduler context)\n", status);
        asm volatile("cli; hlt");
        while(1);
    }
}

int64_t sys_read(int fd, void *buf, size_t count) {
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
                tty_putchar('\n');  // Echo newline
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
                tty_putchar(c);  // Echo the character since callback is removed
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
    
    vfs_file_t *file = (vfs_file_t *)current->fd_table[fd];
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

int64_t sys_write(int fd, const void *buf, size_t count) {
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
    
    vfs_file_t *file = (vfs_file_t *)current->fd_table[fd];
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

int64_t sys_open(const char *path, uint32_t flags, mode_t mode) {
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
        if (current->fd_table[i] == NULL) {
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
    
    current->fd_table[fd] = file;
    return fd;
}

int64_t sys_close(int fd) {
    task_t *current = scheduler_get_current_task();
    if (current == NULL) {
        return -ESRCH;
    }
    
    if (fd < 3 || fd >= MAX_OPEN_FILES) {
        return -EBADF;
    }
    
    vfs_file_t *file = (vfs_file_t *)current->fd_table[fd];
    if (file == NULL) {
        return -EBADF;
    }
    
    int ret = vfs_close(file);
    if (ret != VFS_SUCCESS) {
        return -vfs_errno(ret);
    }
    
    current->fd_table[fd] = NULL;
    return 0;
}

int64_t sys_getpid(void) {
    struct proc *p = proc_get_current();
    if (p == NULL || p->p_p == NULL) {
        return -ESRCH;
    }
    
    return (int64_t)p->p_p->ps_pid;
}

int64_t sys_getppid(void) {
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

int64_t sys_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    struct proc *p = proc_get_current();
    
    tty_printf("[MMAP DEBUG] Called: addr=%p len=%lu prot=%d flags=%d\n", 
               addr, (unsigned long)length, prot, flags);
    
    if (p == NULL) {
        tty_printf("[MMAP DEBUG] No current proc!\n");
        return -ESRCH;
    }
    
    struct process *ps = p->p_p;
    if (ps == NULL || ps->ps_vmspace == NULL) {
        tty_printf("[MMAP DEBUG] No process or vmspace!\n");
        return -ESRCH;
    }
    
    tty_printf("[MMAP DEBUG] Process found: pid=%d\n", ps->ps_pid);

    if (length == 0) {
        return -EINVAL;
    }
    
    if (!(flags & MAP_ANONYMOUS)) {
        return -ENOTSUP;
    }
    
    if (!(flags & MAP_SHARED) && !(flags & MAP_PRIVATE)) {
        return -EINVAL;
    }
    
    size_t aligned_length = (length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    size_t num_pages = aligned_length / PAGE_SIZE;
    uint64_t virt_addr;
    
    if (flags & MAP_FIXED) {
        if (!is_user_address(addr)) {
            return -EINVAL;
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
    
    tty_printf("[MMAP DEBUG] Mapping at virt=0x%lx, %lu pages\n", virt_addr, (unsigned long)num_pages);
    
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
            tty_printf("[MMAP DEBUG] Failed to allocate page %lu\n", (unsigned long)i);
            
            for (size_t j = 0; j < i; j++) {
                paging_unmap_range(ps->ps_vmspace, virt_addr + (j * PAGE_SIZE), 1);
            }
            return -ENOMEM;
        }
        
        memset(page_virt, 0, PAGE_SIZE);
        
        uint64_t phys_page = (uint64_t)page_virt - hhdm_offset;
        
        if (!paging_map_range(ps->ps_vmspace, virt_page, phys_page, 1, page_flags)) {
            tty_printf("[MMAP DEBUG] Failed to map page at 0x%lx\n", virt_page);
            pmm_free(page_virt);
            
            for (size_t j = 0; j < i; j++) {
                paging_unmap_range(ps->ps_vmspace, virt_addr + (j * PAGE_SIZE), 1);
            }
            return -ENOMEM;
        }
        
        tty_printf("[MMAP DEBUG] Mapped page %lu: virt=0x%lx phys=0x%lx\n", 
                   (unsigned long)i, virt_page, phys_page);
    }
    
    if (virt_addr + aligned_length > ps->ps_brk) {
        ps->ps_brk = virt_addr + aligned_length;
    }
    
    tty_printf("[MMAP DEBUG] Success! Returning 0x%lx\n", virt_addr);
    
    return (int64_t)virt_addr;
}

int64_t sys_munmap(void *addr, size_t length) {
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
               virt_addr, (unsigned long)num_pages);
    
    extern struct limine_hhdm_response *boot_get_hhdm(void);
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
    
    tty_printf("[MUNMAP DEBUG] Success!\n");
    
    return 0;
}

int64_t sys_mprotect(void *addr, size_t len, int prot) {
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
    
    if (!paging_protect_range(ps->ps_vmspace, virt_addr, num_pages, page_flags)) {
        return -ENOMEM;
    }
    
    return 0;
}

int64_t sys_gethostname(char *name, size_t len) {
    if (len == 0) {
        return -EINVAL;
    }
    
    if (!is_user_range(name, len)) {
        return -EFAULT;
    }
    
    int ret = gethostname(name, len);
    return (ret < 0) ? ret : 0;
}

int64_t sys_gethostid(void) {
    return gethostid();
}

int64_t sys_sysinfo(struct sysinfo *info) {
    if (!is_user_range(info, sizeof(struct sysinfo))) {
        return -EFAULT;
    }
    
    int ret = kern_sysinfo(info);
    return (ret < 0) ? ret : 0;
}

void syscall_handler(syscall_registers_t *regs) {
    uint64_t syscall_num = regs->rax;
    int64_t ret = -ENOSYS;
    
    switch (syscall_num) {
        case SYSCALL_EXIT:
            sys_exit((int)regs->rdi);
            break;
            
        case SYSCALL_READ:
            ret = sys_read((int)regs->rdi, (void*)regs->rsi, (size_t)regs->rdx);
            break;
            
        case SYSCALL_WRITE:
            ret = sys_write((int)regs->rdi, (const void*)regs->rsi, (size_t)regs->rdx);
            break;
        
        case SYSCALL_OPEN:
            ret = sys_open((const char*)regs->rdi, (uint32_t)regs->rsi, (mode_t)regs->rdx);
            break;
        
        case SYSCALL_CLOSE:
            ret = sys_close((int)regs->rdi);
            break;

        case SYSCALL_GETPID:
            ret = sys_getpid();
            break;

        case SYSCALL_GETPPID:
            ret = sys_getppid();
            break;

        case SYSCALL_MMAP:
            ret = sys_mmap((void*)regs->rdi, (size_t)regs->rsi, (int)regs->rdx,
                       (int)regs->r10, (int)regs->r8, (off_t)regs->r9);
            break;

        case SYSCALL_MUNMAP:
            ret = sys_munmap((void*)regs->rdi, (size_t)regs->rsi);
            break;

        case SYSCALL_MPROTECT:
            ret = sys_mprotect((void*)regs->rdi, (size_t)regs->rsi, (int)regs->rdx);
            break;
        
        case SYSCALL_GETHOSTNAME:
            ret = sys_gethostname((char*)regs->rdi, (size_t)regs->rsi);
            break;

        case SYSCALL_GETHOSTID:
            ret = sys_gethostid();
            break;
        
        case SYSCALL_SYSINFO:
            ret = sys_sysinfo((struct sysinfo*)regs->rdi);
            break;
            
        default:
            tty_printf("[SYSCALL] Unknown syscall: %d\n", (int)syscall_num);
            ret = -ENOSYS;
            break;
    }
    
    regs->rax = (uint64_t)ret;
}

void syscall_init(void) {
    idt_set_gate(SYSCALL_INT, (uint64_t)syscall_stub, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_USER_INT, 0);
    tty_printf("Syscall handler initialized on INT 0x80\n");
}