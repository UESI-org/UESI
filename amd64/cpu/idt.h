#ifndef IDT_H
#define IDT_H

#include <stdint.h>

typedef struct {
    uint16_t offset_low;    // Offset bits 0-15
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;    // Offset bits 16-31
    uint32_t offset_high;   // Offset bits 32-63
    uint32_t zero;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;         // Size of IDT - 1
    uint64_t base;          // Base address of IDT
} __attribute__((packed)) idt_ptr_t;

#define IDT_ENTRIES 256

#define IDT_GATE_INTERRUPT  0x8E    // 64-bit interrupt gate, DPL=0, present
#define IDT_GATE_TRAP       0x8F    // 64-bit trap gate, DPL=0, present
#define IDT_GATE_USER_INT   0xEE    // 64-bit interrupt gate, DPL=3, present
#define IDT_GATE_USER_TRAP  0xEF    // 64-bit trap gate, DPL=3, present

void idt_init(void);

void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags);

extern void idt_load(uint64_t idt_ptr);

#endif