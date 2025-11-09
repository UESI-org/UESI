.set GDT_SELECTOR_KERNEL_CODE, 0x08
.set GDT_SELECTOR_KERNEL_DATA, 0x10

.section .text
.global gdt_load
.global tss_load

gdt_load:
    lgdt (%rdi)              # Load GDT from pointer in RDI
   
    pushq $GDT_SELECTOR_KERNEL_CODE
    leaq .reload_cs(%rip), %rax
    pushq %rax               # Push return address
    lretq                    # Far return to reload CS
    
.reload_cs:
    movw $GDT_SELECTOR_KERNEL_DATA, %ax
    movw %ax, %ds            # Data Segment
    movw %ax, %es            # Extra Segment
    movw %ax, %fs            # FS Segment
    movw %ax, %gs            # GS Segment
    movw %ax, %ss            # Stack Segment
    ret

tss_load:
    ltr %di                  # Load TSS selector into Task Register
    ret