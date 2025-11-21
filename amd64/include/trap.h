#ifndef _MACHINE_TRAP_H_
#define _MACHINE_TRAP_H_

#define T_DIVIDE        0       /* #DE - Divide Error */
#define T_TRCTRAP       1       /* #DB - Debug Exception */
#define T_NMI           2       /* NMI Interrupt */
#define T_BPTFLT        3       /* #BP - Breakpoint */
#define T_OFLOW         4       /* #OF - Overflow */
#define T_BOUND         5       /* #BR - BOUND Range Exceeded */
#define T_PRIVINFLT     6       /* #UD - Invalid Opcode */
#define T_DNA           7       /* #NM - Device Not Available */
#define T_DOUBLEFLT     8       /* #DF - Double Fault */
#define T_FPOPFLT       9       /* Coprocessor Segment Overrun (reserved) */
#define T_TSSFLT        10      /* #TS - Invalid TSS */
#define T_SEGNPFLT      11      /* #NP - Segment Not Present */
#define T_STKFLT        12      /* #SS - Stack-Segment Fault */
#define T_PROTFLT       13      /* #GP - General Protection */
#define T_PAGEFLT       14      /* #PF - Page Fault */
#define T_RESERVED      15      /* Reserved */
#define T_ARITHTRAP     16      /* #MF - x87 FPU Floating-Point Error */
#define T_ALIGNFLT      17      /* #AC - Alignment Check */
#define T_MCA           18      /* #MC - Machine Check */
#define T_XMM           19      /* #XM - SIMD Floating-Point Exception */
#define T_VE            20      /* #VE - Virtualization Exception */
#define T_CP            21      /* #CP - Control Protection Exception */
/* 22-27 reserved */
#define T_HV            28      /* #HV - Hypervisor Injection Exception */
#define T_VC            29      /* #VC - VMM Communication Exception */
#define T_SX            30      /* #SX - Security Exception */
/* 31 reserved */

#define T_EXCEPTION_COUNT   32  /* Number of CPU exceptions */

#define T_IRQ_BASE      32
#define T_IRQ0          (T_IRQ_BASE + 0)    /* PIT Timer */
#define T_IRQ1          (T_IRQ_BASE + 1)    /* Keyboard */
#define T_IRQ2          (T_IRQ_BASE + 2)    /* Cascade */
#define T_IRQ3          (T_IRQ_BASE + 3)    /* COM2 */
#define T_IRQ4          (T_IRQ_BASE + 4)    /* COM1 */
#define T_IRQ5          (T_IRQ_BASE + 5)    /* LPT2 */
#define T_IRQ6          (T_IRQ_BASE + 6)    /* Floppy */
#define T_IRQ7          (T_IRQ_BASE + 7)    /* LPT1 / Spurious */
#define T_IRQ8          (T_IRQ_BASE + 8)    /* CMOS RTC */
#define T_IRQ9          (T_IRQ_BASE + 9)    /* Free / ACPI */
#define T_IRQ10         (T_IRQ_BASE + 10)   /* Free */
#define T_IRQ11         (T_IRQ_BASE + 11)   /* Free */
#define T_IRQ12         (T_IRQ_BASE + 12)   /* PS/2 Mouse */
#define T_IRQ13         (T_IRQ_BASE + 13)   /* FPU */
#define T_IRQ14         (T_IRQ_BASE + 14)   /* Primary ATA */
#define T_IRQ15         (T_IRQ_BASE + 15)   /* Secondary ATA */

#define T_IRQ_COUNT     16

#define T_HAS_ERRCODE(vec) \
    ((vec) == T_DOUBLEFLT || (vec) == T_TSSFLT || (vec) == T_SEGNPFLT || \
     (vec) == T_STKFLT || (vec) == T_PROTFLT || (vec) == T_PAGEFLT || \
     (vec) == T_ALIGNFLT || (vec) == T_CP || (vec) == T_VC || (vec) == T_SX)

#endif