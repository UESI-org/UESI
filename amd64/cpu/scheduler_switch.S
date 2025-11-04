.global scheduler_switch_context
.type scheduler_switch_context, @function

scheduler_switch_context:
    test %rdi, %rdi
    jz load_new_state
    
    mov %rax, 0(%rdi)
    mov %rbx, 8(%rdi)
    mov %rcx, 16(%rdi)
    mov %rdx, 24(%rdi)
    mov %rsi, 32(%rdi)
    mov %rdi, 40(%rdi)
    mov %rbp, 48(%rdi)
    mov %rsp, 56(%rdi)
    mov %r8, 64(%rdi)
    mov %r9, 72(%rdi)
    mov %r10, 80(%rdi)
    mov %r11, 88(%rdi)
    mov %r12, 96(%rdi)
    mov %r13, 104(%rdi)
    mov %r14, 112(%rdi)
    mov %r15, 120(%rdi)
    
    mov (%rsp), %rax
    mov %rax, 128(%rdi)
    
    pushfq
    pop %rax
    mov %rax, 136(%rdi)
    
    mov %cs, %ax
    mov %ax, 144(%rdi)
    mov %ss, %ax
    mov %ax, 146(%rdi)
    
    mov %cr3, %rax
    mov %rax, 148(%rdi)

load_new_state:
    mov 148(%rsi), %rax
    mov %rax, %cr3
    
    mov 0(%rsi), %rax
    mov 8(%rsi), %rbx
    mov 16(%rsi), %rcx
    mov 24(%rsi), %rdx
    mov 40(%rsi), %rdi
    mov 48(%rsi), %rbp
    mov 56(%rsi), %rsp
    mov 64(%rsi), %r8
    mov 72(%rsi), %r9
    mov 80(%rsi), %r10
    mov 88(%rsi), %r11
    mov 96(%rsi), %r12
    mov 104(%rsi), %r13
    mov 112(%rsi), %r14
    mov 120(%rsi), %r15
    
    mov 136(%rsi), %rax
    push %rax
    popfq
    
    push $0x10          # SS
    push 56(%rsi)       # RSP
    pushfq              # RFLAGS
    push $0x08          # CS
    push 128(%rsi)      # RIP
    
    mov 32(%rsi), %rsi
    
    iretq