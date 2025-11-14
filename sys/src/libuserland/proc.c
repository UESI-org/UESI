#include "proc.h"
#include "kmalloc.h"
#include "kfree.h"
#include "paging.h"
#include "pmm.h"
#include "gdt.h"
#include "segments.h"
#include <string.h>
#include <stdbool.h>
#include "printf.h"

static process_t *current_process = NULL;
static pid_t next_pid = 1;

void process_init(void) {
    current_process = NULL;
    next_pid = 1;
    printf_("Process subsystem initialized\n");
}

pid_t process_alloc_pid(void) {
    return next_pid++;
}

process_t *process_create(const char *name) {
    process_t *proc = kmalloc(sizeof(process_t));
    if (!proc) {
        printf_("Failed to allocate process structure\n");
        return NULL;
    }
    
    memset(proc, 0, sizeof(process_t));
    
    /* Set process ID and name */
    proc->pid = process_alloc_pid();
    strncpy(proc->name, name, PROCESS_MAX_NAME - 1);
    proc->name[PROCESS_MAX_NAME - 1] = '\0';
    proc->state = PROCESS_STATE_READY;
    
    /* Create user address space */
    proc->page_dir = paging_create_user_address_space();
    if (!proc->page_dir) {
        printf_("Failed to create address space for process\n");
        kfree(proc);
        return NULL;
    }
    
    proc->kernel_stack = kmalloc(PROCESS_KERNEL_STACK_SIZE);
    if (!proc->kernel_stack) {
        printf_("Failed to allocate kernel stack\n");
        /* TODO: Free page directory */
        kfree(proc);
        return NULL;
    }
    
    memset(proc->kernel_stack, 0, PROCESS_KERNEL_STACK_SIZE);
    proc->kernel_stack_top = (uint64_t)proc->kernel_stack + PROCESS_KERNEL_STACK_SIZE;
    
    /* Set user stack top (will be allocated on-demand or by loader) */
    proc->user_stack_top = USER_STACK_TOP;
    
    printf_("Created process %d (%s)\n", proc->pid, proc->name);
    
    return proc;
}

void process_destroy(process_t *proc) {
    if (!proc) return;
    
    printf_("Destroying process %d (%s)\n", proc->pid, proc->name);
    
    if (proc->kernel_stack) {
        kfree(proc->kernel_stack);
    }
    
    /* TODO: Free user address space and page directory */
    
    kfree(proc);
}

process_t *process_get_current(void) {
    return current_process;
}

void process_set_current(process_t *proc) {
    current_process = proc;
}

extern void enter_usermode_asm(uint64_t entry, uint64_t stack, uint64_t rflags) __attribute__((noreturn));

void process_enter_usermode(process_t *proc, uint64_t entry_point, uint64_t stack_top) {
    extern void serial_printf(uint16_t port, const char *fmt, ...);
    #define DEBUG_PORT 0x3F8
    
    if (!proc) {
        serial_printf(DEBUG_PORT, "ERROR: NULL process!\n");
        printf_("Cannot enter usermode: null process\n");
        while(1) asm("hlt");
    }
    
    serial_printf(DEBUG_PORT, "\n=== PROCESS ENTER USERMODE ===\n");
    serial_printf(DEBUG_PORT, "PID: %d, Name: %s\n", proc->pid, proc->name);
    serial_printf(DEBUG_PORT, "Entry: 0x%016lx\n", entry_point);
    serial_printf(DEBUG_PORT, "Stack: 0x%016lx\n", stack_top);
    
    process_set_current(proc);
    proc->state = PROCESS_STATE_RUNNING;
    
    /* DON'T switch page directory yet! */
    
    /* Set TSS RSP0 for syscalls */
    tss_set_rsp0(proc->kernel_stack_top);
    
    uint64_t user_cs = GDT_SELECTOR_USER_CODE;
    uint64_t user_ss = GDT_SELECTOR_USER_DATA;
    uint64_t rflags = 0x202;
    
    serial_printf(DEBUG_PORT, "CS: 0x%04x, SS: 0x%04x\n", (unsigned int)user_cs, (unsigned int)user_ss);
    serial_printf(DEBUG_PORT, "About to switch to user page tables and execute iretq...\n");
    
    printf_("Entering usermode: entry=0x%lx stack=0x%lx\n", entry_point, stack_top);
    
    if (stack_top & 0xF) {
        stack_top &= ~0xFULL;
        serial_printf(DEBUG_PORT, "Stack aligned to: 0x%016lx\n", stack_top);
    }
    
    /* Get the physical address of the user page directory */
    uint64_t user_cr3 = proc->page_dir->phys_addr;
    
    asm volatile(
        "cli\n"
        
        /* Build iretq frame */
        "movq %2, %%rax\n"
        "pushq %%rax\n"                /* SS */
        
        "movq %1, %%rax\n"
        "pushq %%rax\n"                /* RSP */
        
        "movq %4, %%rax\n"
        "pushq %%rax\n"                /* RFLAGS */
        
        "movq %3, %%rax\n"
        "pushq %%rax\n"                /* CS */
        
        "movq %0, %%rax\n"
        "pushq %%rax\n"                /* RIP */
        
        /* NOW switch to user page directory - right before iretq */
        "movq %5, %%rax\n"
        "movq %%rax, %%cr3\n"          /* Load user CR3 */
        
        /* Load segment registers */
        "movq %2, %%rax\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%fs\n"
        "movw %%ax, %%gs\n"
        
        /* Clear all registers */
        "xorq %%rax, %%rax\n"
        "xorq %%rbx, %%rbx\n"
        "xorq %%rcx, %%rcx\n"
        "xorq %%rdx, %%rdx\n"
        "xorq %%rsi, %%rsi\n"
        "xorq %%rdi, %%rdi\n"
        "xorq %%rbp, %%rbp\n"
        "xorq %%r8, %%r8\n"
        "xorq %%r9, %%r9\n"
        "xorq %%r10, %%r10\n"
        "xorq %%r11, %%r11\n"
        "xorq %%r12, %%r12\n"
        "xorq %%r13, %%r13\n"
        "xorq %%r14, %%r14\n"
        "xorq %%r15, %%r15\n"
        
        "iretq\n"
        :
        : "r"(entry_point), "r"(stack_top), "r"(user_ss), "r"(user_cs), "r"(rflags), "r"(user_cr3)
        : "memory"
    );
    
    __builtin_unreachable();
}