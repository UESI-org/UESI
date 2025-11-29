#include <asm.h>

.macro ISR_NOERRCODE num
    .globl isr\num
    .type isr\num,@function
isr\num:
    endbr64
    cli
    pushq $0
    pushq $\num
    jmp isr_common_stub
    .size isr\num, . - isr\num
.endm

.macro ISR_ERRCODE num
    .globl isr\num
    .type isr\num,@function
isr\num:
    endbr64
    cli
    pushq $\num
    jmp isr_common_stub
    .size isr\num, . - isr\num
.endm

.macro IRQ num, irq_num
    .globl irq\num
    .type irq\num,@function
irq\num:
    endbr64
    cli
    pushq $0
    pushq $\irq_num
    jmp irq_common_stub
    .size irq\num, . - irq\num
.endm

ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8
ISR_NOERRCODE 9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_ERRCODE   17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_ERRCODE   21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_ERRCODE   29
ISR_ERRCODE   30
ISR_NOERRCODE 31

IRQ 0,  32
IRQ 1,  33
IRQ 2,  34
IRQ 3,  35
IRQ 4,  36
IRQ 5,  37
IRQ 6,  38
IRQ 7,  39
IRQ 8,  40
IRQ 9,  41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

ENTRY_NB(isr_common_stub)
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    
    movq %rsp, %rdi
    
    movq %rsp, %rbp
    andq $-16, %rsp
    
    call PIC_PLT(isr_handler)
    
    movq %rbp, %rsp
    
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    
    addq $16, %rsp
    
    iretq
END(isr_common_stub)

ENTRY_NB(irq_common_stub)
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    
    movq %rsp, %rdi
    
    movq %rsp, %rbp
    andq $-16, %rsp
    
    call PIC_PLT(irq_handler)
    
    movq %rbp, %rsp
    
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    
    addq $16, %rsp
    
    iretq
END(irq_common_stub)