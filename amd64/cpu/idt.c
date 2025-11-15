#include <idt.h>
#include <segments.h>
#include <gdt.h>
#include <stddef.h>
#include <string.h>

static struct gate_descriptor idt[IDT_ENTRIES];

static struct region_descriptor idtp;

void idt_set_gate(uint8_t num, uint64_t handler, uint16_t selector, 
                  uint8_t flags, uint8_t ist) {
    // Extract type and DPL from flags byte
    int type = flags & 0x0F;        // Lower 4 bits = type
    int dpl = (flags >> 5) & 0x03;  // Bits 5-6 = DPL
    int present = (flags >> 7) & 0x01; // Bit 7 = present
    
    struct gate_descriptor *gd = &idt[num];
    
    uint64_t offset = handler;
    gd->gd_looffset = offset & 0xFFFF;
    gd->gd_selector = selector;
    gd->gd_ist = ist & 0x7;
    gd->gd_xx1 = 0;
    gd->gd_type = type;
    gd->gd_dpl = dpl;
    gd->gd_p = present;
    gd->gd_hioffset = (offset >> 16) & 0xFFFFFFFFFFFF;
    gd->gd_xx2 = 0;
    gd->gd_zero = 0;
    gd->gd_xx3 = 0;
}

void idt_init(void) {
    // Zero out all IDT entries
    memset(idt, 0, sizeof(idt));
    
    setregion(&idtp, idt, sizeof(idt) - 1);
    
    idt_load((uint64_t)&idtp);
}