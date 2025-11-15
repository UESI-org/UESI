#ifndef GDT_H
#define GDT_H

#include <stdint.h>
#include <segments.h>

/* 
 * GDT Entry indices (not selectors - use GSEL macro for selectors)
 */
#define GDT_NULL_ENTRY      0
#define GDT_KCODE_ENTRY     1
#define GDT_KDATA_ENTRY     2
#define GDT_UCODE_ENTRY     3
#define GDT_UDATA_ENTRY     4
#define GDT_TSS_ENTRY       5

/* Total GDT entries: 5 mem descriptors + 2 for TSS (TSS is 16 bytes) */
#define GDT_ENTRIES         7

/*
 * Segment selectors using OpenBSD-style GSEL macro
 * Format: GSEL(index, privilege_level)
 */
#define GDT_SELECTOR_KERNEL_CODE    GSEL(GDT_KCODE_ENTRY, SEL_KPL)  // 0x08
#define GDT_SELECTOR_KERNEL_DATA    GSEL(GDT_KDATA_ENTRY, SEL_KPL)  // 0x10
#define GDT_SELECTOR_USER_CODE      GSEL(GDT_UCODE_ENTRY, SEL_UPL)  // 0x1B
#define GDT_SELECTOR_USER_DATA      GSEL(GDT_UDATA_ENTRY, SEL_UPL)  // 0x23
#define GDT_SELECTOR_TSS            GSEL(GDT_TSS_ENTRY, SEL_KPL)    // 0x28

/*
 * TSS structure for AMD64 long mode
 */
struct tss_entry {
    uint32_t reserved0;
    uint64_t rsp0;          // Stack pointer for privilege level 0
    uint64_t rsp1;          // Stack pointer for privilege level 1
    uint64_t rsp2;          // Stack pointer for privilege level 2
    uint64_t reserved1;
    uint64_t ist1;          // Interrupt Stack Table 1
    uint64_t ist2;          // Interrupt Stack Table 2
    uint64_t ist3;          // Interrupt Stack Table 3
    uint64_t ist4;          // Interrupt Stack Table 4
    uint64_t ist5;          // Interrupt Stack Table 5
    uint64_t ist6;          // Interrupt Stack Table 6
    uint64_t ist7;          // Interrupt Stack Table 7
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;    // I/O Map Base Address
} __attribute__((packed));

void gdt_init(void);
void gdt_load(struct region_descriptor *gdt_ptr);
void tss_load(uint16_t selector);
void tss_set_rsp0(uint64_t rsp0);

#endif