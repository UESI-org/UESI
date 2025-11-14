.section .text
.global _start
.extern main
.extern exit

_start:
    # Clear frame pointer
    xor %rbp, %rbp
    
    # Align stack to 16 bytes (required by System V ABI)
    and $-16, %rsp
    
    # Call main
    call main
    
    # Exit with return value from main
    mov %rax, %rdi
    call exit
    
    # Should never reach here
    ud2