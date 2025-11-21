#ifndef _MACHINE_ISR_H_
#define _MACHINE_ISR_H_

#include <stdint.h>
#include <trap.h>

extern void isr0(void);     /* T_DIVIDE    - Divide by zero */
extern void isr1(void);     /* T_TRCTRAP   - Debug */
extern void isr2(void);     /* T_NMI       - NMI */
extern void isr3(void);     /* T_BPTFLT    - Breakpoint */
extern void isr4(void);     /* T_OFLOW     - Overflow */
extern void isr5(void);     /* T_BOUND     - Bound range exceeded */
extern void isr6(void);     /* T_PRIVINFLT - Invalid opcode */
extern void isr7(void);     /* T_DNA       - Device not available */
extern void isr8(void);     /* T_DOUBLEFLT - Double fault */
extern void isr9(void);     /* T_FPOPFLT   - Coprocessor segment overrun */
extern void isr10(void);    /* T_TSSFLT    - Invalid TSS */
extern void isr11(void);    /* T_SEGNPFLT  - Segment not present */
extern void isr12(void);    /* T_STKFLT    - Stack-segment fault */
extern void isr13(void);    /* T_PROTFLT   - General protection fault */
extern void isr14(void);    /* T_PAGEFLT   - Page fault */
extern void isr15(void);    /* T_RESERVED  - Reserved */
extern void isr16(void);    /* T_ARITHTRAP - x87 FPU error */
extern void isr17(void);    /* T_ALIGNFLT  - Alignment check */
extern void isr18(void);    /* T_MCA       - Machine check */
extern void isr19(void);    /* T_XMM       - SIMD exception */
extern void isr20(void);    /* T_VE        - Virtualization exception */
extern void isr21(void);    /* T_CP        - Control protection */
extern void isr22(void);    /* Reserved */
extern void isr23(void);    /* Reserved */
extern void isr24(void);    /* Reserved */
extern void isr25(void);    /* Reserved */
extern void isr26(void);    /* Reserved */
extern void isr27(void);    /* Reserved */
extern void isr28(void);    /* T_HV        - Hypervisor injection */
extern void isr29(void);    /* T_VC        - VMM communication */
extern void isr30(void);    /* T_SX        - Security exception */
extern void isr31(void);    /* Reserved */

extern void irq0(void);     /* T_IRQ0  - PIT Timer */
extern void irq1(void);     /* T_IRQ1  - Keyboard */
extern void irq2(void);     /* T_IRQ2  - Cascade */
extern void irq3(void);     /* T_IRQ3  - COM2 */
extern void irq4(void);     /* T_IRQ4  - COM1 */
extern void irq5(void);     /* T_IRQ5  - LPT2 */
extern void irq6(void);     /* T_IRQ6  - Floppy */
extern void irq7(void);     /* T_IRQ7  - LPT1 */
extern void irq8(void);     /* T_IRQ8  - CMOS RTC */
extern void irq9(void);     /* T_IRQ9  - Free/ACPI */
extern void irq10(void);    /* T_IRQ10 - Free */
extern void irq11(void);    /* T_IRQ11 - Free */
extern void irq12(void);    /* T_IRQ12 - PS/2 Mouse */
extern void irq13(void);    /* T_IRQ13 - FPU */
extern void irq14(void);    /* T_IRQ14 - Primary ATA */
extern void irq15(void);    /* T_IRQ15 - Secondary ATA */

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) registers_t;

typedef void (*isr_handler_t)(registers_t *regs);

void isr_install(void);
void isr_register_handler(uint8_t n, isr_handler_t handler);
void isr_unregister_handler(uint8_t n);

#endif