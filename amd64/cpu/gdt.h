#ifndef GDT_H
#define GDT_H

#include <stdint.h>

struct gdt_entry {
    uint16_t limit_low;      // Lower 16 bits of segment limit
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t granularity;     // Upper 4 bits of limit + flags (G, D/B, L, AVL)
    uint8_t base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;          // Size of GDT in bytes minus 1
    uint64_t base;           // Linear address of the GDT
} __attribute__((packed));

struct tss_entry {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed));

#define GDT_ACCESS_PRESENT    (1 << 7)
#define GDT_ACCESS_RING0      (0 << 5)
#define GDT_ACCESS_RING3      (3 << 5)
#define GDT_ACCESS_SYSTEM     (1 << 4)  // System segment (1 = code/data, 0 = TSS/LDT)
#define GDT_ACCESS_EXEC       (1 << 3)  // Executable segment (1 = code, 0 = data)
#define GDT_ACCESS_DC         (1 << 2)  // Direction/Conforming bit
#define GDT_ACCESS_RW         (1 << 1)
#define GDT_ACCESS_ACCESSED   (1 << 0)

#define GDT_GRAN_4K           (1 << 7)  // Granularity (1 = 4KB blocks, 0 = 1B blocks)
#define GDT_GRAN_32BIT        (1 << 6)  // Size flag (1 = 32-bit protected mode)
#define GDT_GRAN_LONG_MODE    (1 << 5)  // Long mode flag (1 = 64-bit mode)

#define GDT_SELECTOR_KERNEL_CODE  0x08  // Offset 8 bytes (entry 1)
#define GDT_SELECTOR_KERNEL_DATA  0x10  // Offset 16 bytes (entry 2)
#define GDT_SELECTOR_USER_CODE    0x18  // Offset 24 bytes (entry 3)
#define GDT_SELECTOR_USER_DATA    0x20  // Offset 32 bytes (entry 4)
#define GDT_SELECTOR_TSS          0x28  // Offset 40 bytes (entry 5)

void gdt_init(void);


void gdt_load(struct gdt_ptr *gdt_ptr);

void tss_load(uint16_t selector);

void tss_set_rsp0(uint64_t rsp0);

#endif