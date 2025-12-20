.section .text
.global _start
.extern main
.extern exit
.extern _init
.extern _fini

_start:
    xor %rbp, %rbp
    
    and $-16, %rsp
    
    mov %rsp, %r15
    
    call _init
    
    mov $_fini, %rdi
    
    call main
    
    mov %rax, %rdi
    
    call _fini
    
    call exit
    
    ud2