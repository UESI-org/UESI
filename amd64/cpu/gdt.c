#include "gdt.h"
#include "segments.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

static struct mem_segment_descriptor gdt_mem[5];
static struct sys_segment_descriptor gdt_tss;

static struct tss_entry tss;

static struct region_descriptor gdt_ptr;

void gdt_init(void) {
    // Zero-initialize the TSS structure
    memset(&tss, 0, sizeof(struct tss_entry));
    tss.iomap_base = sizeof(struct tss_entry);
    
    // Zero-initialize GDT entries
    memset(gdt_mem, 0, sizeof(gdt_mem));
    memset(&gdt_tss, 0, sizeof(gdt_tss));
    
    // Entry 1: Kernel Code Segment (64-bit)
    set_mem_segment(&gdt_mem[GDT_KCODE_ENTRY],
                    NULL,                    // base (ignored in long mode)
                    0xFFFFF,                 // limit (ignored in long mode)
                    SDT_MEMER,               // type: executable, readable
                    SEL_KPL,                 // DPL: kernel privilege
                    1,                       // granularity: 4KB
                    0,                       // def32: not 32-bit
                    1);                      // long mode: yes
    
    // Entry 2: Kernel Data Segment
    set_mem_segment(&gdt_mem[GDT_KDATA_ENTRY],
                    NULL,                    // base (ignored in long mode)
                    0xFFFFF,                 // limit (ignored in long mode)
                    SDT_MEMRW,               // type: read/write data
                    SEL_KPL,                 // DPL: kernel privilege
                    1,                       // granularity: 4KB
                    1,                       // def32: 32-bit
                    0);                      // long mode: no
    
    // Entry 3: User Code Segment (64-bit)
    set_mem_segment(&gdt_mem[GDT_UCODE_ENTRY],
                    NULL,                    // base (ignored in long mode)
                    0xFFFFF,                 // limit (ignored in long mode)
                    SDT_MEMER,               // type: executable, readable
                    SEL_UPL,                 // DPL: user privilege
                    1,                       // granularity: 4KB
                    0,                       // def32: not 32-bit
                    1);                      // long mode: yes
    
    // Entry 4: User Data Segment
    set_mem_segment(&gdt_mem[GDT_UDATA_ENTRY],
                    NULL,                    // base (ignored in long mode)
                    0xFFFFF,                 // limit (ignored in long mode)
                    SDT_MEMRW,               // type: read/write data
                    SEL_UPL,                 // DPL: user privilege
                    1,                       // granularity: 4KB
                    1,                       // def32: 32-bit
                    0);                      // long mode: no
    
    // Entry 5-6: TSS Descriptor (16 bytes in 64-bit mode)
    set_sys_segment(&gdt_tss,
                    &tss,                    // base address of TSS
                    sizeof(struct tss_entry) - 1,  // limit
                    SDT_SYS386TSS,           // type: available TSS
                    SEL_KPL,                 // DPL: kernel privilege
                    0);                      // granularity: byte
    
    // Build a contiguous GDT in memory
    // We need to copy our structures into a single buffer
    static uint8_t gdt_buffer[sizeof(gdt_mem) + sizeof(gdt_tss)];
    memcpy(gdt_buffer, gdt_mem, sizeof(gdt_mem));
    memcpy(gdt_buffer + sizeof(gdt_mem), &gdt_tss, sizeof(gdt_tss));
    
    // Set up the GDT pointer
    setregion(&gdt_ptr, gdt_buffer, sizeof(gdt_buffer) - 1);
    
    gdt_load(&gdt_ptr);
    
    tss_load(GDT_SELECTOR_TSS);
}

void tss_set_rsp0(uint64_t rsp0) {
    tss.rsp0 = rsp0;
}