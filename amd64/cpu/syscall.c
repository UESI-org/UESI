#include <stdbool.h>
#include "syscall.h"

extern bool keyboard_has_key(void);
extern char keyboard_getchar(void);
extern void tty_printf(const char *fmt, ...);
extern void tty_putchar(char c);
extern void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags);
extern void syscall_stub(void);

int64_t sys_read(int fd, void *buf, size_t count) {
    if (fd != 0) {
        return -1;
    }
    
    if (buf == NULL || count == 0) {
        return -1;
    }
    
    char *buffer = (char *)buf;
    size_t bytes_read = 0;
    
    // Read characters from keyboard buffer
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
        
        // Handle backspace (Ctrl+H or ASCII 8)
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

int64_t sys_write(int fd, const void *buf, size_t count) {
    if (fd == 1 || fd == 2) { // stdout or stderr
        const char *str = (const char *)buf;
        for (size_t i = 0; i < count; i++) {
            tty_putchar(str[i]);
        }
        return count;
    }
    return -1;
}

void sys_exit(int status) {
    tty_printf("\nProcess exited with status: %d\n", status);
    asm volatile("cli; hlt");
    while(1);
}

void syscall_handler(syscall_registers_t *regs) {
    // System V ABI for syscalls on x86_64:
    // rax = syscall number
    // rdi = arg1, rsi = arg2, rdx = arg3
    // Return value goes in rax
    
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
            
        default:
            tty_printf("Unknown syscall: %d\n", (int)syscall_num);
            ret = -1;
            break;
    }
    
    regs->rax = (uint64_t)ret;
}

void syscall_init(void) {
    idt_set_gate(SYSCALL_INT, (uint64_t)syscall_stub, 0x08, 0xEE); // 0xEE = Ring 3 accessible
    tty_printf("Syscall handler initialized on INT 0x80\n");
}