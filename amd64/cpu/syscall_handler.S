.global syscall_stub
.extern syscall_handler

.section .text

syscall_stub:
    cli
    
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
    
    push $0          # err_code (syscalls don't have error codes)
    push $0x80       # int_no
    
    mov %rsp, %rdi
    call syscall_handler
    
    add $16, %rsp
    
    pop %rax
    pop %rbx
    pop %rcx
    pop %rdx
    pop %rsi
    pop %rdi
    pop %rbp
    add $8, %rsp     # Skip saved rsp
    pop %r8
    pop %r9
    pop %r10
    pop %r11
    pop %r12
    pop %r13
    pop %r14
    pop %r15
    
    sti
    iretq