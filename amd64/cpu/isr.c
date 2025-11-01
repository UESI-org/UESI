#include <stddef.h>
#include "isr.h"
#include "io.h"
#include "pit.h"
#include "mmu.h"

extern void tty_printf(const char *fmt, ...);
extern void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags);

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1
#define PIC_EOI      0x20

static const char *exception_messages[] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 FPU Error",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Hypervisor Injection Exception",
    "VMM Communication Exception",
    "Security Exception",
    "Reserved"
};

static isr_handler_t interrupt_handlers[256] = {0};

void isr_handler(registers_t *regs) {
    if (interrupt_handlers[regs->int_no] != NULL) {
        isr_handler_t handler = interrupt_handlers[regs->int_no];
        handler(regs);
    } else {
        if (regs->int_no < 32) {
            // Special handling for page faults
            if (regs->int_no == 14) {
                uint64_t faulting_address;
                asm volatile("mov %%cr2, %0" : "=r"(faulting_address));
                mmu_handle_page_fault(faulting_address, regs->err_code);
            }
            
            tty_printf("\n=== EXCEPTION: %s ===\n", exception_messages[regs->int_no]);
            tty_printf("INT: %d, ERR: 0x%p\n", regs->int_no, (void*)regs->err_code);
            tty_printf("RAX: 0x%p  RBX: 0x%p\n", (void*)regs->rax, (void*)regs->rbx);
            tty_printf("RCX: 0x%p  RDX: 0x%p\n", (void*)regs->rcx, (void*)regs->rdx);
            tty_printf("RSI: 0x%p  RDI: 0x%p\n", (void*)regs->rsi, (void*)regs->rdi);
            tty_printf("RBP: 0x%p  RSP: 0x%p\n", (void*)regs->rbp, (void*)regs->rsp);
            tty_printf("RIP: 0x%p  RFLAGS: 0x%p\n", (void*)regs->rip, (void*)regs->rflags);
            tty_printf("CS: 0x%x  SS: 0x%x\n", (unsigned int)regs->cs, (unsigned int)regs->ss);
            
            if (regs->int_no == 14) {
                uint64_t faulting_address;
                asm volatile("mov %%cr2, %0" : "=r"(faulting_address));
                tty_printf("Page fault at: 0x%p\n", (void*)faulting_address);
                tty_printf("Error code: ");
                if (regs->err_code & 0x1) tty_printf("PRESENT ");
                if (regs->err_code & 0x2) tty_printf("WRITE ");
                if (regs->err_code & 0x4) tty_printf("USER ");
                if (regs->err_code & 0x8) tty_printf("RESERVED ");
                if (regs->err_code & 0x10) tty_printf("INSTRUCTION_FETCH ");
                tty_printf("\n");
            }
     
            asm volatile("cli; hlt");
            while(1);
        }
    }
}

void irq_handler(registers_t *regs) {
    if (interrupt_handlers[regs->int_no] != NULL) {
        isr_handler_t handler = interrupt_handlers[regs->int_no];
        handler(regs);
    }
    
    if (regs->int_no >= 40) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}

void isr_register_handler(uint8_t n, isr_handler_t handler) {
    interrupt_handlers[n] = handler;
}

void isr_unregister_handler(uint8_t n) {
    interrupt_handlers[n] = NULL;
}

void isr_install() {
    idt_set_gate(0, (uint64_t)isr0, 0x08, 0x8E);
    idt_set_gate(1, (uint64_t)isr1, 0x08, 0x8E);
    idt_set_gate(2, (uint64_t)isr2, 0x08, 0x8E);
    idt_set_gate(3, (uint64_t)isr3, 0x08, 0x8E);
    idt_set_gate(4, (uint64_t)isr4, 0x08, 0x8E);
    idt_set_gate(5, (uint64_t)isr5, 0x08, 0x8E);
    idt_set_gate(6, (uint64_t)isr6, 0x08, 0x8E);
    idt_set_gate(7, (uint64_t)isr7, 0x08, 0x8E);
    idt_set_gate(8, (uint64_t)isr8, 0x08, 0x8E);
    idt_set_gate(9, (uint64_t)isr9, 0x08, 0x8E);
    idt_set_gate(10, (uint64_t)isr10, 0x08, 0x8E);
    idt_set_gate(11, (uint64_t)isr11, 0x08, 0x8E);
    idt_set_gate(12, (uint64_t)isr12, 0x08, 0x8E);
    idt_set_gate(13, (uint64_t)isr13, 0x08, 0x8E);
    idt_set_gate(14, (uint64_t)isr14, 0x08, 0x8E);
    idt_set_gate(15, (uint64_t)isr15, 0x08, 0x8E);
    idt_set_gate(16, (uint64_t)isr16, 0x08, 0x8E);
    idt_set_gate(17, (uint64_t)isr17, 0x08, 0x8E);
    idt_set_gate(18, (uint64_t)isr18, 0x08, 0x8E);
    idt_set_gate(19, (uint64_t)isr19, 0x08, 0x8E);
    idt_set_gate(20, (uint64_t)isr20, 0x08, 0x8E);
    idt_set_gate(21, (uint64_t)isr21, 0x08, 0x8E);
    idt_set_gate(22, (uint64_t)isr22, 0x08, 0x8E);
    idt_set_gate(23, (uint64_t)isr23, 0x08, 0x8E);
    idt_set_gate(24, (uint64_t)isr24, 0x08, 0x8E);
    idt_set_gate(25, (uint64_t)isr25, 0x08, 0x8E);
    idt_set_gate(26, (uint64_t)isr26, 0x08, 0x8E);
    idt_set_gate(27, (uint64_t)isr27, 0x08, 0x8E);
    idt_set_gate(28, (uint64_t)isr28, 0x08, 0x8E);
    idt_set_gate(29, (uint64_t)isr29, 0x08, 0x8E);
    idt_set_gate(30, (uint64_t)isr30, 0x08, 0x8E);
    idt_set_gate(31, (uint64_t)isr31, 0x08, 0x8E);
    
    idt_set_gate(32, (uint64_t)irq0, 0x08, 0x8E);
    idt_set_gate(33, (uint64_t)irq1, 0x08, 0x8E);
    idt_set_gate(34, (uint64_t)irq2, 0x08, 0x8E);
    idt_set_gate(35, (uint64_t)irq3, 0x08, 0x8E);
    idt_set_gate(36, (uint64_t)irq4, 0x08, 0x8E);
    idt_set_gate(37, (uint64_t)irq5, 0x08, 0x8E);
    idt_set_gate(38, (uint64_t)irq6, 0x08, 0x8E);
    idt_set_gate(39, (uint64_t)irq7, 0x08, 0x8E);
    idt_set_gate(40, (uint64_t)irq8, 0x08, 0x8E);
    idt_set_gate(41, (uint64_t)irq9, 0x08, 0x8E);
    idt_set_gate(42, (uint64_t)irq10, 0x08, 0x8E);
    idt_set_gate(43, (uint64_t)irq11, 0x08, 0x8E);
    idt_set_gate(44, (uint64_t)irq12, 0x08, 0x8E);
    idt_set_gate(45, (uint64_t)irq13, 0x08, 0x8E);
    idt_set_gate(46, (uint64_t)irq14, 0x08, 0x8E);
    idt_set_gate(47, (uint64_t)irq15, 0x08, 0x8E);
}