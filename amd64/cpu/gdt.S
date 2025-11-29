#include <asm.h>

.set GDT_SELECTOR_KERNEL_CODE, 0x08
.set GDT_SELECTOR_KERNEL_DATA, 0x10

ENTRY(gdt_load)
    RETGUARD_SETUP(gdt_load, r11)
    lgdt (%rdi)
   
    pushq $GDT_SELECTOR_KERNEL_CODE
    leaq .reload_cs(%rip), %rax
    pushq %rax
    lretq
    
.reload_cs:
    movw $GDT_SELECTOR_KERNEL_DATA, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    RETGUARD_CHECK(gdt_load, r11)
    ret
    lfence
END(gdt_load)

ENTRY(tss_load)
    RETGUARD_SETUP(tss_load, r11)
    ltr %di
    RETGUARD_CHECK(tss_load, r11)
    ret
    lfence
END(tss_load)