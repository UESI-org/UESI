.section .text
.global gdt_load
.global tss_load

gdt_load:
    # Load GDT
    lgdt (%rdi)
    
    pushq $0x08              # Push kernel code segment selector
    leaq .reload_cs(%rip), %rax
    pushq %rax               # Push return address
    lretq                    # Far return to reload CS
    
.reload_cs:
    movw $0x10, %ax          # Kernel data segment selector
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    ret

tss_load:
    ltr %di                  # Load TSS selector into task register
    ret
