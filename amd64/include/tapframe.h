#ifndef _MACHINE_TRAPFRAME_H_
#define _MACHINE_TRAPFRAME_H_

#include <stdint.h>

/*
 * Trapframe structure pushed by interrupt/exception handlers
 * 
 * Layout matches the order of pushes in isr_common:
 * 1. General purpose registers (pushed by isr_common in reverse order)
 * 2. Vector number and error code (pushed by ISR stub)
 * 3. CPU state (pushed automatically by CPU on exception/interrupt)
 *
 * Total size: 176 bytes (22 * 8 bytes)
 */
struct trapframe {
	/* Offset 0-119: General purpose registers (pushed by isr_common) */
	uint64_t tf_r15;     /*   0 */
	uint64_t tf_r14;     /*   8 */
	uint64_t tf_r13;     /*  16 */
	uint64_t tf_r12;     /*  24 */
	uint64_t tf_r11;     /*  32 */
	uint64_t tf_r10;     /*  40 */
	uint64_t tf_r9;      /*  48 */
	uint64_t tf_r8;      /*  56 */
	uint64_t tf_rbp;     /*  64 */
	uint64_t tf_rdi;     /*  72 */
	uint64_t tf_rsi;     /*  80 */
	uint64_t tf_rdx;     /*  88 */
	uint64_t tf_rcx;     /*  96 */
	uint64_t tf_rbx;     /* 104 */
	uint64_t tf_rax;     /* 112 */

	/* Offset 120-135: Trap information (pushed by ISR stub) */
	uint64_t tf_trapno;  /* 120 - vector number */
	uint64_t tf_err;     /* 128 - error code (or 0) */

	/* Offset 136-175: CPU-pushed state on exception/interrupt */
	uint64_t tf_rip;     /* 136 - instruction pointer */
	uint64_t tf_cs;      /* 144 - code segment */
	uint64_t tf_rflags;  /* 152 - CPU flags */
	uint64_t tf_rsp;     /* 160 - stack pointer (only on privilege change) */
	uint64_t tf_ss;      /* 168 - stack segment (only on privilege change) */
} __attribute__((packed));

/*
 * Assembly offset validation macros
 * Use these in assembly to ensure offsets match C structure
 */
#define TF_R15     0
#define TF_R14     8
#define TF_R13     16
#define TF_R12     24
#define TF_R11     32
#define TF_R10     40
#define TF_R9      48
#define TF_R8      56
#define TF_RBP     64
#define TF_RDI     72
#define TF_RSI     80
#define TF_RDX     88
#define TF_RCX     96
#define TF_RBX     104
#define TF_RAX     112
#define TF_TRAPNO  120
#define TF_ERR     128
#define TF_RIP     136
#define TF_CS      144
#define TF_RFLAGS  152
#define TF_RSP     160
#define TF_SS      168

#define TF_SIZE    176  /* Total size of trapframe */

#endif /* _MACHINE_TRAPFRAME_H_ */