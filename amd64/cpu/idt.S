#include <asm.h>

ENTRY(idt_load)
    RETGUARD_SETUP(idt_load, r11)
    lidt (%rdi)
    RETGUARD_CHECK(idt_load, r11)
    ret
    lfence
END(idt_load)