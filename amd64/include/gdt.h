#ifndef GDT_H
#define GDT_H

#include <stdint.h>
#include <segments.h>
#include <tss.h>

#define GDT_NULL_ENTRY 0
#define GDT_KCODE_ENTRY 1
#define GDT_KDATA_ENTRY 2
#define GDT_UDATA_ENTRY 3
#define GDT_UCODE_ENTRY 4
#define GDT_TSS_ENTRY 5

#define GDT_ENTRIES 7

#define GDT_SELECTOR_KERNEL_CODE GSEL(GDT_KCODE_ENTRY, SEL_KPL) // 0x08
#define GDT_SELECTOR_KERNEL_DATA GSEL(GDT_KDATA_ENTRY, SEL_KPL) // 0x10
#define GDT_SELECTOR_USER_DATA GSEL(GDT_UDATA_ENTRY, SEL_UPL)   // 0x18
#define GDT_SELECTOR_USER_CODE GSEL(GDT_UCODE_ENTRY, SEL_UPL)   // 0x20
#define GDT_SELECTOR_TSS GSEL(GDT_TSS_ENTRY, SEL_KPL)           // 0x28

void gdt_init(void);
void gdt_load(struct region_descriptor *gdt_ptr);
void tss_load(uint16_t selector);
void tss_set_rsp0(uint64_t rsp0);

#endif