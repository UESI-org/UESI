.section .text
.global _start
.extern main
.extern exit
.extern _init
.extern _fini

_start:
    xor %rbp, %rbp
    
    and $-16, %rsp
    
    call _init
    
    xor %rdi, %rdi          # rdi = 0 (argc = 0)
    xor %rsi, %rsi          # rsi = NULL (argv = NULL)
    
    call main
    
    call _fini
    
    mov %rax, %rdi          # Pass main's return value as exit code
    call exit
    
    ud2