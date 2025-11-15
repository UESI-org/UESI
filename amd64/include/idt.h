#ifndef IDT_H
#define IDT_H

#include <stdint.h>
#include <segments.h>

#define IDT_ENTRIES 256

/*
 * Gate type flags for IDT entries
 * These combine the type field with DPL and present bit
 */
#define IDT_GATE_INTERRUPT  0x8E    // Interrupt gate, DPL=0, present (0b10001110)
#define IDT_GATE_TRAP       0x8F    // Trap gate, DPL=0, present (0b10001111)
#define IDT_GATE_USER_INT   0xEE    // Interrupt gate, DPL=3, present (0b11101110)
#define IDT_GATE_USER_TRAP  0xEF    // Trap gate, DPL=3, present (0b11101111)

void idt_init(void);
void idt_set_gate(uint8_t num, uint64_t handler, uint16_t selector, 
                  uint8_t flags, uint8_t ist);
void idt_load(uint64_t idt_ptr);

#endif