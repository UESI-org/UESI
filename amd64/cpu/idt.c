#include <idt.h>
#include <segments.h>
#include <gdt.h>
#include <stddef.h>
#include <string.h>

static struct gate_descriptor idt[IDT_ENTRIES];
static struct region_descriptor idtp;

void idt_set_gate(uint8_t vec, uint64_t handler, uint16_t selector, 
                  uint8_t flags, uint8_t ist) {
    int type = flags & 0x0F;
    int dpl = (flags >> 5) & 0x03;
    int present = (flags >> 7) & 0x01;
    
    struct gate_descriptor *gd = &idt[vec];
    
    gd->gd_looffset = handler & 0xFFFF;
    gd->gd_selector = selector;
    gd->gd_ist = ist & 0x7;
    gd->gd_xx1 = 0;
    gd->gd_type = type;
    gd->gd_dpl = dpl;
    gd->gd_p = present;
    gd->gd_hioffset = (handler >> 16) & 0xFFFFFFFFFFFF;
    gd->gd_xx2 = 0;
    gd->gd_zero = 0;
    gd->gd_xx3 = 0;
}

void idt_init(void) {
    memset(idt, 0, sizeof(idt));
    setregion(&idtp, idt, sizeof(idt) - 1);
    idt_load((uint64_t)&idtp);
}