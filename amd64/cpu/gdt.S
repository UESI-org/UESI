.section .text
.global gdt_load
.global tss_load

gdt_load:
    # Load GDT pointer into GDTR register
    lgdt (%rdi)
   
    pushq $0x08              # Push kernel code segment selector (entry 1)
    leaq .reload_cs(%rip), %rax
    pushq %rax               # Push return address
    lretq                    # Far return to reload CS
    
.reload_cs:
    # Reload all data segment registers with kernel data selector
    movw $0x10, %ax          # Kernel data segment selector (entry 2)
    movw %ax, %ds            # Data Segment
    movw %ax, %es            # Extra Segment
    movw %ax, %fs            # FS Segment
    movw %ax, %gs            # GS Segment
    movw %ax, %ss            # Stack Segment
    ret

tss_load:
    ltr %di                  # Load TSS selector into Task Register
    ret