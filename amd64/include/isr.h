#ifndef ISR_H
#define ISR_H

#include <stdint.h>

extern void isr0();   // Divide by zero
extern void isr1();   // Debug
extern void isr2();   // NMI
extern void isr3();   // Breakpoint
extern void isr4();   // Overflow
extern void isr5();   // Bound range exceeded
extern void isr6();   // Invalid opcode
extern void isr7();   // Device not available
extern void isr8();   // Double fault
extern void isr9();   // Coprocessor segment overrun
extern void isr10();  // Invalid TSS
extern void isr11();  // Segment not present
extern void isr12();  // Stack-segment fault
extern void isr13();  // General protection fault
extern void isr14();  // Page fault
extern void isr15();  // Reserved
extern void isr16();  // x87 FPU error
extern void isr17();  // Alignment check
extern void isr18();  // Machine check
extern void isr19();  // SIMD floating-point exception
extern void isr20();  // Virtualization exception
extern void isr21();  // Control protection exception
extern void isr22();  // Reserved
extern void isr23();  // Reserved
extern void isr24();  // Reserved
extern void isr25();  // Reserved
extern void isr26();  // Reserved
extern void isr27();  // Reserved
extern void isr28();  // Hypervisor injection exception
extern void isr29();  // VMM communication exception
extern void isr30();  // Security exception
extern void isr31();  // Reserved

extern void irq0();   // Timer
extern void irq1();   // Keyboard
extern void irq2();   // Cascade
extern void irq3();   // COM2
extern void irq4();   // COM1
extern void irq5();   // LPT2
extern void irq6();   // Floppy
extern void irq7();   // LPT1
extern void irq8();   // CMOS RTC
extern void irq9();   // Free
extern void irq10();  // Free
extern void irq11();  // Free
extern void irq12();  // PS/2 Mouse
extern void irq13();  // FPU
extern void irq14();  // Primary ATA
extern void irq15();  // Secondary ATA

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) registers_t;

typedef void (*isr_handler_t)(registers_t *regs);

void isr_install();

void isr_register_handler(uint8_t n, isr_handler_t handler);

void isr_unregister_handler(uint8_t n);

#endif