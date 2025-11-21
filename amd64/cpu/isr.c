#include <stddef.h>
#include <isr.h>
#include <idt.h>
#include <trap.h>
#include <gdt.h>
#include <io.h>
#include <mmu.h>
#include <panic.h>

extern void tty_printf(const char *fmt, ...);

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1
#define PIC_EOI      0x20

static const char *exception_messages[] = {
    [T_DIVIDE]    = "Division By Zero",
    [T_TRCTRAP]   = "Debug",
    [T_NMI]       = "Non Maskable Interrupt",
    [T_BPTFLT]    = "Breakpoint",
    [T_OFLOW]     = "Overflow",
    [T_BOUND]     = "Bound Range Exceeded",
    [T_PRIVINFLT] = "Invalid Opcode",
    [T_DNA]       = "Device Not Available",
    [T_DOUBLEFLT] = "Double Fault",
    [T_FPOPFLT]   = "Coprocessor Segment Overrun",
    [T_TSSFLT]    = "Invalid TSS",
    [T_SEGNPFLT]  = "Segment Not Present",
    [T_STKFLT]    = "Stack-Segment Fault",
    [T_PROTFLT]   = "General Protection Fault",
    [T_PAGEFLT]   = "Page Fault",
    [T_RESERVED]  = "Reserved",
    [T_ARITHTRAP] = "x87 FPU Error",
    [T_ALIGNFLT]  = "Alignment Check",
    [T_MCA]       = "Machine Check",
    [T_XMM]       = "SIMD Floating-Point Exception",
    [T_VE]        = "Virtualization Exception",
    [T_CP]        = "Control Protection Exception",
    [22] = "Reserved", [23] = "Reserved", [24] = "Reserved",
    [25] = "Reserved", [26] = "Reserved", [27] = "Reserved",
    [T_HV]        = "Hypervisor Injection Exception",
    [T_VC]        = "VMM Communication Exception",
    [T_SX]        = "Security Exception",
    [31]          = "Reserved"
};

static isr_handler_t interrupt_handlers[IDT_ENTRIES] = {0};

void isr_handler(registers_t *regs) {
    if (interrupt_handlers[regs->int_no] != NULL) {
        interrupt_handlers[regs->int_no](regs);
        return;
    }
    
    if (regs->int_no < T_EXCEPTION_COUNT) {
        panic_code_t code;
        
        switch (regs->int_no) {
        case T_DIVIDE:    code = PANIC_DIVIDE_BY_ZERO; break;
        case T_PRIVINFLT: code = PANIC_INVALID_OPCODE; break;
        case T_DOUBLEFLT: code = PANIC_DOUBLE_FAULT; break;
        case T_STKFLT:    code = PANIC_STACK_OVERFLOW; break;
        case T_PROTFLT:   code = PANIC_GPF; break;
        case T_PAGEFLT:   code = PANIC_PAGE_FAULT; break;
        case T_MCA:       code = PANIC_MACHINE_CHECK; break;
        default:          code = PANIC_GENERAL; break;
        }
        
        if (regs->int_no == T_PAGEFLT) {
            uint64_t fault_addr;
            __asm__ volatile("mov %%cr2, %0" : "=r"(fault_addr));
            
            mmu_handle_page_fault(fault_addr, regs->err_code);
            
            char pf_msg[256];
            char *p = pf_msg;
            const char *msg1 = "Page Fault at address 0x";
            while (*msg1) *p++ = *msg1++;
            
            for (int i = 60; i >= 0; i -= 4) {
                int digit = (fault_addr >> i) & 0xF;
                *p++ = "0123456789ABCDEF"[digit];
            }
            
            const char *msg2 = " - ";
            while (*msg2) *p++ = *msg2++;
            
            const char *reason;
            if (!(regs->err_code & 0x1))
                reason = "Page not present";
            else if (regs->err_code & 0x2)
                reason = "Write to read-only page";
            else if (regs->err_code & 0x4)
                reason = "User mode access violation";
            else if (regs->err_code & 0x8)
                reason = "Reserved bit violation";
            else if (regs->err_code & 0x10)
                reason = "Instruction fetch";
            else
                reason = "Unknown";
            
            while (*reason) *p++ = *reason++;
            *p = '\0';
            
            panic_registers(pf_msg, regs, code);
        }
        
        panic_registers(exception_messages[regs->int_no], regs, code);
    }
}

void irq_handler(registers_t *regs) {
    if (interrupt_handlers[regs->int_no] != NULL) {
        interrupt_handlers[regs->int_no](regs);
    }
    
    /* Send EOI to slave PIC if IRQ came from it (IRQ 8-15) */
    if (regs->int_no >= T_IRQ8) {
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

void isr_install(void) {
    idt_set_gate(T_DIVIDE,    (uint64_t)isr0,  GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(T_TRCTRAP,   (uint64_t)isr1,  GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(T_NMI,       (uint64_t)isr2,  GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, IDT_IST_NMI);
    idt_set_gate(T_BPTFLT,    (uint64_t)isr3,  GDT_SELECTOR_KERNEL_CODE, IDT_GATE_USER_TRAP, 0);
    idt_set_gate(T_OFLOW,     (uint64_t)isr4,  GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(T_BOUND,     (uint64_t)isr5,  GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(T_PRIVINFLT, (uint64_t)isr6,  GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(T_DNA,       (uint64_t)isr7,  GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(T_DOUBLEFLT, (uint64_t)isr8,  GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, IDT_IST_DOUBLEFLT);
    idt_set_gate(T_FPOPFLT,   (uint64_t)isr9,  GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(T_TSSFLT,    (uint64_t)isr10, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(T_SEGNPFLT,  (uint64_t)isr11, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(T_STKFLT,    (uint64_t)isr12, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(T_PROTFLT,   (uint64_t)isr13, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(T_PAGEFLT,   (uint64_t)isr14, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(T_RESERVED,  (uint64_t)isr15, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(T_ARITHTRAP, (uint64_t)isr16, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(T_ALIGNFLT,  (uint64_t)isr17, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(T_MCA,       (uint64_t)isr18, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, IDT_IST_MCE);
    idt_set_gate(T_XMM,       (uint64_t)isr19, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(T_VE,        (uint64_t)isr20, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(T_CP,        (uint64_t)isr21, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(22,          (uint64_t)isr22, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(23,          (uint64_t)isr23, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(24,          (uint64_t)isr24, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(25,          (uint64_t)isr25, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(26,          (uint64_t)isr26, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(27,          (uint64_t)isr27, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(T_HV,        (uint64_t)isr28, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(T_VC,        (uint64_t)isr29, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(T_SX,        (uint64_t)isr30, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(31,          (uint64_t)isr31, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    
    idt_set_gate(T_IRQ0,  (uint64_t)irq0,  GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(T_IRQ1,  (uint64_t)irq1,  GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(T_IRQ2,  (uint64_t)irq2,  GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(T_IRQ3,  (uint64_t)irq3,  GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(T_IRQ4,  (uint64_t)irq4,  GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(T_IRQ5,  (uint64_t)irq5,  GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(T_IRQ6,  (uint64_t)irq6,  GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(T_IRQ7,  (uint64_t)irq7,  GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(T_IRQ8,  (uint64_t)irq8,  GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(T_IRQ9,  (uint64_t)irq9,  GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(T_IRQ10, (uint64_t)irq10, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(T_IRQ11, (uint64_t)irq11, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(T_IRQ12, (uint64_t)irq12, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(T_IRQ13, (uint64_t)irq13, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(T_IRQ14, (uint64_t)irq14, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
    idt_set_gate(T_IRQ15, (uint64_t)irq15, GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
}