#ifndef _MACHINE_TRAPFRAME_H_
#define _MACHINE_TRAPFRAME_H_

#include <stdint.h>

struct trapframe {
	/* Pushed by isr_common (reverse order) */
	uint64_t tf_r15;
	uint64_t tf_r14;
	uint64_t tf_r13;
	uint64_t tf_r12;
	uint64_t tf_r11;
	uint64_t tf_r10;
	uint64_t tf_r9;
	uint64_t tf_r8;
	uint64_t tf_rbp;
	uint64_t tf_rdi;
	uint64_t tf_rsi;
	uint64_t tf_rdx;
	uint64_t tf_rcx;
	uint64_t tf_rbx;
	uint64_t tf_rax;

	/* Pushed by ISR stub */
	uint64_t tf_trapno; /* vector number */
	uint64_t tf_err;    /* error code (or 0) */

	/* Pushed by CPU on exception */
	uint64_t tf_rip;
	uint64_t tf_cs;
	uint64_t tf_rflags;
	uint64_t tf_rsp; /* only valid on privilege change */
	uint64_t tf_ss;  /* only valid on privilege change */
};

#endif