.section .text
.global idt_load

idt_load:
    lidt (%rdi)                 // Load IDT from pointer in RDI
    ret