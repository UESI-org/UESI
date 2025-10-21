#include "gdt.h"
#include <stdint.h>
#include <stddef.h>

static struct gdt_entry gdt[7];
static struct tss_entry tss;
static struct gdt_ptr gdt_ptr;

static void gdt_set_entry(int index, uint32_t base, uint32_t limit, uint8_t access, uint8_t granularity) {
    gdt[index].base_low = (base & 0xFFFF);
    gdt[index].base_mid = (base >> 16) & 0xFF;
    gdt[index].base_high = (base >> 24) & 0xFF;
    
    gdt[index].limit_low = (limit & 0xFFFF);
    gdt[index].granularity = (limit >> 16) & 0x0F;
    gdt[index].granularity |= granularity & 0xF0;
    gdt[index].access = access;
}

static void gdt_set_tss(int index, uint64_t base, uint32_t limit) {
    gdt[index].limit_low = limit & 0xFFFF;
    gdt[index].base_low = base & 0xFFFF;
    gdt[index].base_mid = (base >> 16) & 0xFF;
    gdt[index].access = 0x89; // Present, Ring 0, TSS Available
    gdt[index].granularity = 0x00;
    gdt[index].base_high = (base >> 24) & 0xFF;
    
    struct gdt_entry *upper = &gdt[index + 1];
    uint64_t base_upper = base >> 32;
    upper->limit_low = base_upper & 0xFFFF;
    upper->base_low = (base_upper >> 16) & 0xFFFF;
    upper->base_mid = 0;
    upper->access = 0;
    upper->granularity = 0;
    upper->base_high = 0;
}

void gdt_init(void) {
    for (size_t i = 0; i < sizeof(struct tss_entry); i++) {
        ((uint8_t *)&tss)[i] = 0;
    }
    tss.iomap_base = sizeof(struct tss_entry);
    
    gdt_ptr.limit = (sizeof(struct gdt_entry) * 7) - 1;
    gdt_ptr.base = (uint64_t)&gdt;
    
    gdt_set_entry(0, 0, 0, 0, 0);
    
    gdt_set_entry(1, 0, 0xFFFFF,
                  GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 | GDT_ACCESS_SYSTEM | GDT_ACCESS_EXEC | GDT_ACCESS_RW,
                  GDT_GRAN_4K | GDT_GRAN_LONG_MODE);
    
    gdt_set_entry(2, 0, 0xFFFFF,
                  GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 | GDT_ACCESS_SYSTEM | GDT_ACCESS_RW,
                  GDT_GRAN_4K | GDT_GRAN_32BIT);
    
    gdt_set_entry(3, 0, 0xFFFFF,
                  GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 | GDT_ACCESS_SYSTEM | GDT_ACCESS_EXEC | GDT_ACCESS_RW,
                  GDT_GRAN_4K | GDT_GRAN_LONG_MODE);
    
    gdt_set_entry(4, 0, 0xFFFFF,
                  GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 | GDT_ACCESS_SYSTEM | GDT_ACCESS_RW,
                  GDT_GRAN_4K | GDT_GRAN_32BIT);
    
    gdt_set_tss(5, (uint64_t)&tss, sizeof(struct tss_entry) - 1);
    
    gdt_load(&gdt_ptr);
    
    tss_load(GDT_SELECTOR_TSS);
}

void tss_set_rsp0(uint64_t rsp0) {
    tss.rsp0 = rsp0;
}