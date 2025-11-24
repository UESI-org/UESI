#include <stdbool.h>
#include <syscall.h>
#include <scheduler.h>
#include <vfs.h>
#include <idt.h>
#include <gdt.h>
#include <sys/sysinfo.h>

extern bool keyboard_has_key(void);
extern char keyboard_getchar(void);
extern void tty_printf(const char *fmt, ...);
extern void tty_putchar(char c);
extern void syscall_stub(void);

extern int kern_sysinfo(struct sysinfo *info);
extern int kern_sysinfo(struct sysinfo *info);
extern int gethostname(char *name, size_t len);
extern int gethostid(void);

int64_t sys_sysinfo(struct sysinfo *info) {
    if (info == NULL) {
        return -1;
    }
    
    int ret = kern_sysinfo(info);
    
    return ret;
}

int64_t sys_gethostname(char *name, size_t len) {
    if (name == NULL || len == 0) {
        return -1;
    }
    
    int ret = gethostname(name, len);
    
    if (ret == 0) {
        tty_printf("[SYSCALL] gethostname returned: %s\n", name);
    }
    
    return ret;
}

int64_t sys_gethostid(void) {
    int ret = gethostid();
    tty_printf("[SYSCALL] gethostid returned: %d\n", ret);
    return ret;
}

int64_t sys_open(const char *path, uint32_t flags, mode_t mode) {
    task_t *current = scheduler_get_current_task();
    if (current == NULL || path == NULL) {
        return -1;
    }
    
    // Find free fd slot (start at 3, skip stdin/stdout/stderr)
    int fd = -1;
    for (int i = 3; i < MAX_OPEN_FILES; i++) {
        if (current->fd_table[i] == NULL) {
            fd = i;
            break;
        }
    }
    
    if (fd == -1) {
        tty_printf("[SYSCALL] No free file descriptors\n");
        return -1; // No free file descriptors (EMFILE)
    }
    
    vfs_file_t *file;
    int ret = vfs_open(path, flags, mode, &file);
    if (ret != 0) {
        tty_printf("[SYSCALL] vfs_open failed: %d\n", ret);
        return ret; // Return VFS error code
    }
    
    current->fd_table[fd] = file;
    
    tty_printf("[SYSCALL] Task %d opened '%s' as fd %d\n", 
               current->tid, path, fd);
    
    return fd;
}

int64_t sys_close(int fd) {
    task_t *current = scheduler_get_current_task();
    if (current == NULL) {
        return -1;
    }
    
    // Can't close stdin/stdout/stderr
    if (fd < 3 || fd >= MAX_OPEN_FILES) {
        return -1; // Invalid fd
    }
    
    vfs_file_t *file = (vfs_file_t *)current->fd_table[fd];
    if (file == NULL) {
        return -1; // File not open
    }
    
    int ret = vfs_close(file);
    if (ret != 0) {
        tty_printf("[SYSCALL] vfs_close failed: %d\n", ret);
        return ret;
    }
    
    current->fd_table[fd] = NULL;
    
    tty_printf("[SYSCALL] Task %d closed fd %d\n", current->tid, fd);
    
    return 0;
}

int64_t sys_read(int fd, void *buf, size_t count) {
    if (buf == NULL || count == 0) {
        return -1;
    }
    
    // Handle stdin (fd 0) - read from keyboard
    if (fd == 0) {
        char *buffer = (char *)buf;
        size_t bytes_read = 0;
        
        while (bytes_read < count) {
            if (!keyboard_has_key()) {
                if (bytes_read > 0) {
                    break;
                }
                while (!keyboard_has_key()) {
                    asm volatile("hlt");
                }
            }
            
            char c = keyboard_getchar();
            
            if (c == '\n' || c == '\r') {
                buffer[bytes_read++] = '\n';
                tty_putchar('\n');
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
            
            buffer[bytes_read++] = c;
            tty_putchar(c);
        }
        
        return bytes_read;
    }
    
    task_t *current = scheduler_get_current_task();
    if (current == NULL) {
        return -1; // No current task
    }
    
    if (fd < 0 || fd >= MAX_OPEN_FILES) {
        return -1; // Invalid fd
    }
    
    vfs_file_t *file = (vfs_file_t *)current->fd_table[fd];
    if (file == NULL) {
        return -1; // File not open
    }
    
    ssize_t result = vfs_read(file, buf, count);
    return result;
}

int64_t sys_write(int fd, const void *buf, size_t count) {
    if (buf == NULL || count == 0) {
        return -1;
    }
    
    // Handle stdout/stderr (fd 1 and 2) - direct TTY output
    if (fd == 1 || fd == 2) {
        const char *str = (const char *)buf;
        for (size_t i = 0; i < count; i++) {
            tty_putchar(str[i]);
        }
        return count;
    }
    
    task_t *current = scheduler_get_current_task();
    if (current == NULL) {
        return -1; // No current task
    }
    
    if (fd < 0 || fd >= MAX_OPEN_FILES) {
        return -1; // Invalid fd
    }
    
    vfs_file_t *file = (vfs_file_t *)current->fd_table[fd];
    if (file == NULL) {
        return -1; // File not open
    }
    
    ssize_t result = vfs_write(file, buf, count);
    return result;
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

void syscall_handler(syscall_registers_t *regs) {
    uint64_t syscall_num = regs->rax;
    int64_t ret = -1;
    
    switch (syscall_num) {
        case SYSCALL_READ:
            ret = sys_read((int)regs->rdi, (void*)regs->rsi, (size_t)regs->rdx);
            break;
            
        case SYSCALL_WRITE:
            ret = sys_write((int)regs->rdi, (const void*)regs->rsi, (size_t)regs->rdx);
            break;
            
        case SYSCALL_EXIT:
            sys_exit((int)regs->rdi);
            break;
        
        case SYSCALL_OPEN:
            ret = sys_open((const char*)regs->rdi, (uint32_t)regs->rsi, (mode_t)regs->rdx);
            break;
        
        case SYSCALL_CLOSE:
            ret = sys_close((int)regs->rdi);
            break;
        
        case SYSCALL_SYSINFO:
            ret = sys_sysinfo((struct sysinfo*)regs->rdi);
            break;
        
        case SYSCALL_GETHOSTNAME:
            ret = sys_gethostname((char*)regs->rdi, (size_t)regs->rsi);
            break;

        case SYSCALL_GETHOSTID:
            ret = sys_gethostid();
            break;
            
        default:
            tty_printf("Unknown syscall: %d\n", (int)syscall_num);
            ret = -1;
            break;
    }
    
    regs->rax = (uint64_t)ret;
}

void syscall_init(void) {
    idt_set_gate(SYSCALL_INT, (uint64_t)syscall_stub, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_USER_INT, 0);
    tty_printf("Syscall handler initialized on INT 0x80\n");
}