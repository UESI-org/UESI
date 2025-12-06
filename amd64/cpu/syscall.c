#include <stdbool.h>
#include <syscall.h>
#include <scheduler.h>
#include <vfs.h>
#include <idt.h>
#include <gdt.h>
#include <sys/sysinfo.h>
#include <sys/errno.h>
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

static inline int vfs_errno(int vfs_ret) {
    if (vfs_ret == VFS_SUCCESS) return 0;
    if (vfs_ret < 0) return -vfs_ret;
    return EIO;
}

void sys_exit(int status) {
    task_t *current = scheduler_get_current_task();
    
    if (current) {
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