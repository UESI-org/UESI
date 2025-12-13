#include <segments.h>
#include <stddef.h>
#include <string.h>

void
setgate(struct gate_descriptor *gd,
        void *func,
        int ist,
        int type,
        int dpl,
        int selector)
{
	uint64_t offset = (uint64_t)func;

	gd->gd_looffset = offset & 0xFFFF;
	gd->gd_selector = selector;
	gd->gd_ist = ist & 0x7;
	gd->gd_xx1 = 0;
	gd->gd_type = type;
	gd->gd_dpl = dpl;
	gd->gd_p = 1; /* present */
	gd->gd_hioffset = (offset >> 16) & 0xFFFFFFFFFFFF;
	gd->gd_xx2 = 0;
	gd->gd_zero = 0;
	gd->gd_xx3 = 0;
}

/*
 * Clear a gate descriptor
 */
void
unsetgate(struct gate_descriptor *gd)
{
	memset(gd, 0, sizeof(*gd));
}

void
setregion(struct region_descriptor *rd, void *base, uint16_t limit)
{
	rd->rd_limit = limit;
	rd->rd_base = (uint64_t)base;
}

void
set_sys_segment(struct sys_segment_descriptor *sd,
                void *base,
                uint64_t limit,
                int type,
                int dpl,
                int gran)
{
	uint64_t addr = (uint64_t)base;

	sd->sd_lolimit = limit & 0xFFFF;
	sd->sd_lobase = addr & 0xFFFFFF;
	sd->sd_type = type;
	sd->sd_dpl = dpl;
	sd->sd_p = 1; /* present */
	sd->sd_hilimit = (limit >> 16) & 0xF;
	sd->sd_xx1 = 0;
	sd->sd_gran = gran;
	sd->sd_hibase = (addr >> 24) & 0xFFFFFFFFFF;
	sd->sd_xx2 = 0;
	sd->sd_zero = 0;
	sd->sd_xx3 = 0;
}

void
set_mem_segment(struct mem_segment_descriptor *sd,
                void *base,
                uint64_t limit,
                int type,
                int dpl,
                int gran,
                int def32,
                int is_long)
{
	uint64_t addr = (uint64_t)base;

	sd->sd_lolimit = limit & 0xFFFF;
	sd->sd_lobase = addr & 0xFFFFFF;
	sd->sd_type = type;
	sd->sd_dpl = dpl;
	sd->sd_p = 1; /* present */
	sd->sd_hilimit = (limit >> 16) & 0xF;
	sd->sd_avl = 0;
	sd->sd_long = is_long;
	sd->sd_def32 = def32;
	sd->sd_gran = gran;
	sd->sd_hibase = (addr >> 24) & 0xFF;
}