#include <panic.h>
#include <tty.h>
#include <serial.h>
#include <io.h>
#include <stdarg.h>

#define PANIC_BUFFER_SIZE       512
#define PANIC_MAX_STACK_FRAMES  16
#define PANIC_SERIAL_PORT       SERIAL_COM1

#define KERNEL_STACK_TOP        0xFFFFFFFF80000000ULL
#define KERNEL_STACK_BOTTOM     0xFFFFFFFFFFFFFFFFULL

static volatile bool in_panic = false;
static char panic_buffer[PANIC_BUFFER_SIZE];

__attribute__((noreturn))
static inline void halt_cpu(void) {
    __asm__ volatile (
        "cli\n"           /* Disable interrupts */
        "1:\n"
        "hlt\n"           /* Halt CPU */
        "jmp 1b\n"        /* Loop forever in case of NMI */
        ::: "memory"
    );
    __builtin_unreachable();
}

static inline void disable_interrupts(void) {
    __asm__ volatile ("cli" ::: "memory");
}

static void serial_write_direct(uint16_t port, const char *str) {
    while (*str) {
        /* Wait for transmit buffer to be empty */
        while (!(inb(port + SERIAL_LINE_STATUS_REG) & SERIAL_LSR_TRANSMIT_EMPTY))
            ;
        outb(port + SERIAL_DATA_REG, (uint8_t)*str);
        str++;
    }
}

static void panic_output(const char *str) {
    tty_writestring(str);
    tty_writestring("\n");
    
    serial_write_string(PANIC_SERIAL_PORT, str);
    serial_write_string(PANIC_SERIAL_PORT, "\r\n");
    
    serial_write_direct(PANIC_SERIAL_PORT, str);
    serial_write_direct(PANIC_SERIAL_PORT, "\r\n");
}

static void panic_printf(const char *fmt, ...) {
    va_list args;
    char buffer[256];
    
    va_start(args, fmt);
    
    const char *p = fmt;
    char *out = buffer;
    char *end = buffer + sizeof(buffer) - 1;
    
    while (*p && out < end) {
        if (*p == '%' && *(p + 1)) {
            p++;
            if (*p == 's') {
                const char *s = va_arg(args, const char*);
                if (s) {
                    while (*s && out < end) *out++ = *s++;
                } else {
                    const char *null = "(null)";
                    while (*null && out < end) *out++ = *null++;
                }
            } else if (*p == 'p' || *p == 'x') {
                uint64_t val = va_arg(args, uint64_t);
                if (*p == 'p' && out + 2 < end) {
                    *out++ = '0';
                    *out++ = 'x';
                }
                char hex[20];
                int i = 0;
                if (val == 0) {
                    hex[i++] = '0';
                } else {
                    for (int shift = 60; shift >= 0; shift -= 4) {
                        int digit = (val >> shift) & 0xF;
                        if (i > 0 || digit != 0) {
                            hex[i++] = "0123456789ABCDEF"[digit];
                        }
                    }
                }
                hex[i] = '\0';
                for (int j = 0; j < i && out < end; j++) {
                    *out++ = hex[j];
                }
            } else if (*p == 'd') {
                int val = va_arg(args, int);
                if (val < 0) {
                    *out++ = '-';
                    val = -val;
                }
                char dec[20];
                int i = 0;
                if (val == 0) {
                    dec[i++] = '0';
                } else {
                    while (val > 0) {
                        dec[i++] = '0' + (val % 10);
                        val /= 10;
                    }
                }
                /* Reverse */
                for (int j = i - 1; j >= 0 && out < end; j--) {
                    *out++ = dec[j];
                }
            } else {
                *out++ = *p;
            }
            p++;
        } else {
            *out++ = *p++;
        }
    }
    *out = '\0';
    
    va_end(args);
    
    panic_output(buffer);
}

static bool is_valid_address(uint64_t addr) {
    if (addr == 0 || addr < 0x1000) return false;
    if (addr >= KERNEL_STACK_TOP) return true;
    if (addr < 0x100000000ULL) return true;
    return false;
}

static void dump_registers(registers_t *regs) {
    panic_output("=== CPU REGISTERS ===");
    
    tty_printf("RAX: 0x%016llx  RBX: 0x%016llx\n", regs->rax, regs->rbx);
    tty_printf("RCX: 0x%016llx  RDX: 0x%016llx\n", regs->rcx, regs->rdx);
    tty_printf("RSI: 0x%016llx  RDI: 0x%016llx\n", regs->rsi, regs->rdi);
    tty_printf("RBP: 0x%016llx  RSP: 0x%016llx\n", regs->rbp, regs->rsp);
    tty_printf("R8:  0x%016llx  R9:  0x%016llx\n", regs->r8, regs->r9);
    tty_printf("R10: 0x%016llx  R11: 0x%016llx\n", regs->r10, regs->r11);
    tty_printf("R12: 0x%016llx  R13: 0x%016llx\n", regs->r12, regs->r13);
    tty_printf("R14: 0x%016llx  R15: 0x%016llx\n", regs->r14, regs->r15);
    tty_printf("RIP: 0x%016llx  RFLAGS: 0x%016llx\n", regs->rip, regs->rflags);
    tty_printf("CS:  0x%04x          SS:  0x%04x\n", (uint16_t)regs->cs, (uint16_t)regs->ss);
    tty_printf("Interrupt: %llu      Error Code: 0x%016llx\n", regs->int_no, regs->err_code);
    
    serial_printf(PANIC_SERIAL_PORT, "RAX: 0x%016llx  RBX: 0x%016llx\r\n", regs->rax, regs->rbx);
    serial_printf(PANIC_SERIAL_PORT, "RCX: 0x%016llx  RDX: 0x%016llx\r\n", regs->rcx, regs->rdx);
    serial_printf(PANIC_SERIAL_PORT, "RSI: 0x%016llx  RDI: 0x%016llx\r\n", regs->rsi, regs->rdi);
    serial_printf(PANIC_SERIAL_PORT, "RBP: 0x%016llx  RSP: 0x%016llx\r\n", regs->rbp, regs->rsp);
    serial_printf(PANIC_SERIAL_PORT, "RIP: 0x%016llx  RFLAGS: 0x%016llx\r\n", regs->rip, regs->rflags);
}

static void dump_stack_trace(uint64_t rbp, uint64_t rip) {
    int frame = 0;
    
    panic_output("");
    panic_output("=== STACK TRACE ===");
    
    tty_printf("#0:  0x%016llx\n", rip);
    serial_printf(PANIC_SERIAL_PORT, "#0:  0x%016llx\r\n", rip);
    
    uint64_t *frame_ptr = (uint64_t*)rbp;
    
    for (frame = 1; frame < PANIC_MAX_STACK_FRAMES; frame++) {
        if (!is_valid_address((uint64_t)frame_ptr)) break;
        if (frame_ptr == NULL || (uint64_t)frame_ptr < 0x1000) break;
        if ((uint64_t)frame_ptr == rbp) break;
        
        uint64_t ret_addr = frame_ptr[1];
        uint64_t next_rbp = frame_ptr[0];
        
        if (ret_addr == 0 || ret_addr < 0x1000) break;
        
        tty_printf("#%d:  0x%016llx\n", frame, ret_addr);
        serial_printf(PANIC_SERIAL_PORT, "#%d:  0x%016llx\r\n", frame, ret_addr);
        
        if (!is_valid_address(next_rbp) || next_rbp <= (uint64_t)frame_ptr) break;
        
        frame_ptr = (uint64_t*)next_rbp;
    }
    
    if (frame >= PANIC_MAX_STACK_FRAMES) {
        panic_output("(stack trace truncated)");
    }
}

static void print_panic_banner(void) {
    tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_RED);
    tty_clear();
    
    panic_output("================================================================================");
    panic_output("!!!                             KERNEL PANIC                                 !!!");
    panic_output("!!!                             SYSTEM HALTED                                !!!");
    panic_output("================================================================================");
    panic_output("");
}

__attribute__((noreturn))
void panic(const char *message) {
    /* Prevent re-entrant panics */
    if (in_panic) {
        halt_cpu();
    }
    in_panic = true;
    
    disable_interrupts();
    
    print_panic_banner();
    
    panic_output("PANIC:");
    if (message != NULL && is_valid_address((uint64_t)message)) {
        panic_output(message);
    } else {
        panic_output("(invalid message pointer)");
    }
    panic_output("");
    
    panic_output("================================================================================");
    panic_output("                            System halted. Please reboot.");
    panic_output("================================================================================");
    
    halt_cpu();
}

__attribute__((noreturn))
void panic_fmt(const char *fmt, ...) {
    va_list args;
    
    /* Prevent re-entrant panics */
    if (in_panic) {
        halt_cpu();
    }
    in_panic = true;
    
    disable_interrupts();
    
    print_panic_banner();
    
    panic_output("PANIC:");
    va_start(args, fmt);
    
    tty_writestring("  ");
    
    const char *p = fmt;
    while (*p) {
        if (*p == '%' && *(p + 1)) {
            p++;
            if (*p == 's') {
                const char *s = va_arg(args, const char*);
                tty_writestring(s ? s : "(null)");
            } else if (*p == 'd') {
                int val = va_arg(args, int);
                tty_printf("%d", val);
            } else if (*p == 'u') {
                unsigned int val = va_arg(args, unsigned int);
                tty_printf("%u", val);
            } else if (*p == 'x') {
                unsigned long val = va_arg(args, unsigned long);
                tty_printf("%llx", (unsigned long long)val);
            } else if (*p == 'p') {
                void *val = va_arg(args, void*);
                tty_printf("%p", val);
            } else if (*p == 'l' && *(p+1) == 'l') {
                p += 2;
                if (*p == 'u') {
                    unsigned long long val = va_arg(args, unsigned long long);
                    tty_printf("%llu", val);
                } else if (*p == 'x') {
                    unsigned long long val = va_arg(args, unsigned long long);
                    tty_printf("%llx", val);
                } else if (*p == 'd') {
                    long long val = va_arg(args, long long);
                    tty_printf("%lld", val);
                }
            } else {
                tty_putchar(*p);
            }
            p++;
        } else {
            tty_putchar(*p++);
        }
    }
    tty_putchar('\n');
    
    va_end(args);
    panic_output("");
    
    panic_output("================================================================================");
    panic_output("                            System halted. Please reboot.");
    panic_output("================================================================================");
    
    halt_cpu();
}

__attribute__((noreturn))
void panic_registers(const char *message, registers_t *regs, panic_code_t code) {
    /* Prevent re-entrant panics */
    if (in_panic) {
        halt_cpu();
    }
    in_panic = true;
    
    disable_interrupts();
    
    print_panic_banner();
    
    panic_output("PANIC:");
    if (message != NULL && is_valid_address((uint64_t)message)) {
        panic_printf("  %s", message);
    } else {
        panic_output("  (invalid message pointer)");
    }
    
    panic_printf("Error Type: %s", panic_code_name(code));
    panic_output("");
    
    if (regs != NULL && is_valid_address((uint64_t)regs)) {
        dump_registers(regs);
        
        dump_stack_trace(regs->rbp, regs->rip);
    } else {
        panic_output("(register dump unavailable)");
    }
    
    panic_output("");
    
    panic_output("================================================================================");
    panic_output("                            System halted. Please reboot.");
    panic_output("================================================================================");
    
    halt_cpu();
}

__attribute__((noreturn))
void panic_assert(const char *expr, const char *file, int line, const char *func) {
    /* Prevent re-entrant panics */
    if (in_panic) {
        halt_cpu();
    }
    in_panic = true;
    
    disable_interrupts();
    
    print_panic_banner();
    
    panic_output("ASSERTION FAILED!");
    panic_output("");
    
    if (expr != NULL && is_valid_address((uint64_t)expr)) {
        panic_printf("Expression: %s", expr);
    }
    
    if (file != NULL && is_valid_address((uint64_t)file)) {
        panic_printf("File: %s:%d", file, line);
    }
    
    if (func != NULL && is_valid_address((uint64_t)func)) {
        panic_printf("Function: %s", func);
    }
    
    panic_output("");
    
    uint64_t rbp;
    __asm__ volatile ("mov %%rbp, %0" : "=r"(rbp));
    uint64_t rip = (uint64_t)panic_assert;
    
    dump_stack_trace(rbp, rip);
    panic_output("");
    
    panic_output("================================================================================");
    panic_output("                            System halted. Please reboot.");
    panic_output("================================================================================");
    
    halt_cpu();
}

bool panic_in_progress(void) {
    return in_panic;
}

const char *panic_code_name(panic_code_t code) {
    switch (code) {
        case PANIC_GENERAL:             return "General Panic";
        case PANIC_HARDWARE_FAULT:      return "Hardware Fault";
        case PANIC_MEMORY_CORRUPTION:   return "Memory Corruption";
        case PANIC_ASSERTION_FAILED:    return "Assertion Failed";
        case PANIC_STACK_OVERFLOW:      return "Stack Overflow";
        case PANIC_DIVIDE_BY_ZERO:      return "Division by Zero";
        case PANIC_PAGE_FAULT:          return "Page Fault";
        case PANIC_DOUBLE_FAULT:        return "Double Fault";
        case PANIC_INVALID_OPCODE:      return "Invalid Opcode";
        case PANIC_GPF:                 return "General Protection Fault";
        case PANIC_MACHINE_CHECK:       return "Machine Check";
        case PANIC_OUT_OF_MEMORY:       return "Out of Memory";
        case PANIC_DEADLOCK:            return "Deadlock Detected";
        case PANIC_UNKNOWN:             return "Unknown Error";
        default:                        return "Undefined Panic Code";
    }
}