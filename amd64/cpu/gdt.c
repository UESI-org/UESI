#include <gdt.h>
#include <segments.h>
#include <tss.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

static struct mem_segment_descriptor gdt_mem[5];
static struct sys_segment_descriptor gdt_tss;

static struct x86_64_tss tss;

static struct region_descriptor gdt_ptr;

void
gdt_init(void)
{
	memset(&tss, 0, sizeof(struct x86_64_tss));
	tss.tss_iobase = sizeof(struct x86_64_tss);

	memset(gdt_mem, 0, sizeof(gdt_mem));
	memset(&gdt_tss, 0, sizeof(gdt_tss));

	// Entry 1: Kernel Code Segment (64-bit)
	// Access: 0x9A = P=1, DPL=0, S=1, E=1, DC=0, RW=1, A=0
	// Flags: 0xA = G=1, DB=0, L=1 (long mode)
	set_mem_segment(&gdt_mem[GDT_KCODE_ENTRY],
	                NULL,      // base (ignored in long mode)
	                0xFFFFF,   // limit (ignored in long mode)
	                SDT_MEMER, // type: executable, readable
	                SEL_KPL,   // DPL: kernel privilege
	                1,         // granularity: 4KB
	                0,         // def32: 0 for 64-bit
	                1);        // long mode: yes

	// Entry 2: Kernel Data Segment
	// Access: 0x92 = P=1, DPL=0, S=1, E=0, DC=0, RW=1, A=0
	// Flags: 0xC = G=1, DB=1, L=0
	set_mem_segment(&gdt_mem[GDT_KDATA_ENTRY],
	                NULL,      // base (ignored in long mode)
	                0xFFFFF,   // limit (ignored in long mode)
	                SDT_MEMRW, // type: read/write data
	                SEL_KPL,   // DPL: kernel privilege
	                1,         // granularity: 4KB
	                1,         // def32: 32-bit
	                0);        // long mode: no

	// Entry 3: User Data Segment
	// Access: 0xF2 = P=1, DPL=3, S=1, E=0, DC=0, RW=1, A=0
	// Flags: 0xC = G=1, DB=1, L=0
	set_mem_segment(&gdt_mem[GDT_UDATA_ENTRY],
	                NULL,      // base (ignored in long mode)
	                0xFFFFF,   // limit (ignored in long mode)
	                SDT_MEMRW, // type: read/write data
	                SEL_UPL,   // DPL: user privilege
	                1,         // granularity: 4KB
	                1,         // def32: 32-bit
	                0);        // long mode: no

	// Entry 4: User Code Segment (64-bit)
	// Access: 0xFA = P=1, DPL=3, S=1, E=1, DC=0, RW=1, A=0
	// Flags: 0xA = G=1, DB=0, L=1 (long mode)
	set_mem_segment(&gdt_mem[GDT_UCODE_ENTRY],
	                NULL,      // base (ignored in long mode)
	                0xFFFFF,   // limit (ignored in long mode)
	                SDT_MEMER, // type: executable, readable
	                SEL_UPL,   // DPL: user privilege
	                1,         // granularity: 4KB
	                0,         // def32: not 32-bit
	                1);        // long mode: yes

	// Entry 5-6: TSS Descriptor (16 bytes in 64-bit mode)
	// Access: 0x89 = P=1, DPL=0, Type=0x9 (64-bit TSS Available)
	// Flags: 0x0 = G=0 (byte granularity for TSS)
	set_sys_segment(&gdt_tss,
	                &tss,                          // base address of TSS
	                sizeof(struct x86_64_tss) - 1, // limit
	                SDT_SYS386TSS,                 // type: available TSS
	                SEL_KPL,                       // DPL: kernel privilege
	                0);                            // granularity: byte

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

void
tss_set_rsp0(uint64_t rsp0)
{
	tss.tss_rsp0 = rsp0;
}