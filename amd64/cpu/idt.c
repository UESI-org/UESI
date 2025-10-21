#include "idt.h"
#include "gdt.h"
#include <stddef.h>

static idt_entry_t idt[IDT_ENTRIES];

static idt_ptr_t idtp;

void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags) {
    idt[num].offset_low = base & 0xFFFF;
    idt[num].offset_mid = (base >> 16) & 0xFFFF;
    idt[num].offset_high = (base >> 32) & 0xFFFFFFFF;
    
    idt[num].selector = sel;
    idt[num].ist = 0;           // Don't use IST for now
    idt[num].type_attr = flags;
    idt[num].zero = 0;
}

void idt_init(void) {
    idtp.limit = (sizeof(idt_entry_t) * IDT_ENTRIES) - 1;
    idtp.base = (uint64_t)&idt;
    
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt[i].offset_low = 0;
        idt[i].offset_mid = 0;
        idt[i].offset_high = 0;
        idt[i].selector = 0;
        idt[i].ist = 0;
        idt[i].type_attr = 0;
        idt[i].zero = 0;
    }
    
    idt_load((uint64_t)&idtp);
}