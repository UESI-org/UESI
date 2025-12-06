#include <asm.h>

ENTRY(syscall_stub)
    push %r15
    push %r14
    push %r13
    push %r12
    push %r11
    push %r10
    push %r9
    push %r8
    push %rsp
    push %rbp
    push %rdi
    push %rsi
    push %rdx
    push %rcx
    push %rbx
    push %rax
    
    push $0
    push $0x80
    
    mov %rsp, %rdi
    
    sti
    call PIC_PLT(syscall_handler)
    cli
    
    add $16, %rsp
    
    pop %rax
    pop %rbx
    pop %rcx
    pop %rdx
    pop %rsi
    pop %rdi
    pop %rbp
    add $8, %rsp
    pop %r8
    pop %r9
    pop %r10
    pop %r11
    pop %r12
    pop %r13
    pop %r14
    pop %r15
    
    iretq
END(syscall_stub)