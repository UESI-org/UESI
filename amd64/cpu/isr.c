#include <stddef.h>
#include <isr.h>
#include <idt.h>
#include <gdt.h>
#include <io.h>
#include <pit.h>
#include <mmu.h>
#include <panic.h>

extern void tty_printf(const char *fmt, ...);

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
            /* Determine panic code based on exception type */
            panic_code_t code = PANIC_GENERAL;
            
            switch (regs->int_no) {
                case 0:  code = PANIC_DIVIDE_BY_ZERO; break;
                case 6:  code = PANIC_INVALID_OPCODE; break;
                case 8:  code = PANIC_DOUBLE_FAULT; break;
                case 12: code = PANIC_STACK_OVERFLOW; break;
                case 13: code = PANIC_GPF; break;
                case 14: code = PANIC_PAGE_FAULT; break;
                case 18: code = PANIC_MACHINE_CHECK; break;
                default: code = PANIC_GENERAL; break;
            }
            
            /* Special handling for page faults - try MMU handler first */
            if (regs->int_no == 14) {
                uint64_t faulting_address;
                __asm__ volatile("mov %%cr2, %0" : "=r"(faulting_address));
                
                mmu_handle_page_fault(faulting_address, regs->err_code);
                
                char pf_msg[256];
                char *p = pf_msg;
                
                const char *msg1 = "Page Fault at address 0x";
                while (*msg1) *p++ = *msg1++;
                
                for (int i = 60; i >= 0; i -= 4) {
                    int digit = (faulting_address >> i) & 0xF;
                    *p++ = "0123456789ABCDEF"[digit];
                }
                
                const char *msg2 = " - ";
                while (*msg2) *p++ = *msg2++;
                
                if (!(regs->err_code & 0x1)) {
                    const char *msg = "Page not present";
                    while (*msg) *p++ = *msg++;
                } else if (regs->err_code & 0x2) {
                    const char *msg = "Write to read-only page";
                    while (*msg) *p++ = *msg++;
                } else if (regs->err_code & 0x4) {
                    const char *msg = "User mode access violation";
                    while (*msg) *p++ = *msg++;
                } else if (regs->err_code & 0x8) {
                    const char *msg = "Reserved bit violation";
                    while (*msg) *p++ = *msg++;
                } else if (regs->err_code & 0x10) {
                    const char *msg = "Instruction fetch";
                    while (*msg) *p++ = *msg++;
                }
                *p = '\0';
                
                panic_registers(pf_msg, regs, code);
            }
            
            panic_registers(exception_messages[regs->int_no], regs, code);
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
    idt_set_gate(0, (uint64_t)isr0, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(1, (uint64_t)isr1, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(2, (uint64_t)isr2, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(3, (uint64_t)isr3, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(4, (uint64_t)isr4, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(5, (uint64_t)isr5, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(6, (uint64_t)isr6, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(7, (uint64_t)isr7, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(8, (uint64_t)isr8, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(9, (uint64_t)isr9, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(10, (uint64_t)isr10, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(11, (uint64_t)isr11, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(12, (uint64_t)isr12, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(13, (uint64_t)isr13, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(14, (uint64_t)isr14, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(15, (uint64_t)isr15, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(16, (uint64_t)isr16, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(17, (uint64_t)isr17, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(18, (uint64_t)isr18, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(19, (uint64_t)isr19, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(20, (uint64_t)isr20, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(21, (uint64_t)isr21, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(22, (uint64_t)isr22, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(23, (uint64_t)isr23, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(24, (uint64_t)isr24, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(25, (uint64_t)isr25, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(26, (uint64_t)isr26, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(27, (uint64_t)isr27, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(28, (uint64_t)isr28, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(29, (uint64_t)isr29, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(30, (uint64_t)isr30, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(31, (uint64_t)isr31, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    
    idt_set_gate(32, (uint64_t)irq0, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(33, (uint64_t)irq1, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(34, (uint64_t)irq2, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(35, (uint64_t)irq3, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(36, (uint64_t)irq4, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(37, (uint64_t)irq5, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(38, (uint64_t)irq6, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(39, (uint64_t)irq7, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(40, (uint64_t)irq8, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(41, (uint64_t)irq9, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(42, (uint64_t)irq10, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(43, (uint64_t)irq11, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(44, (uint64_t)irq12, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(45, (uint64_t)irq13, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(46, (uint64_t)irq14, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(47, (uint64_t)irq15, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
}