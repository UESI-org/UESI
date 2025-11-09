#ifndef _MACHINE_SEGMENTS_H_
#define _MACHINE_SEGMENTS_H_

#include <stdint.h>

/*
 * Selectors
 */

#define ISPL(s)         ((s) & SEL_RPL)    /* what is the priority level of a selector */
#define SEL_KPL         0                   /* kernel privilege level */
#define SEL_UPL         3                   /* user privilege level */
#define SEL_RPL         3                   /* requester's privilege level mask */
#define ISLDT(s)        ((s) & SEL_LDT)    /* is it local or global */
#define SEL_LDT         4                   /* local descriptor table */

#define IDXSEL(s)       (((s) >> 3) & 0x1fff)

#define GSEL(s,r)       (((s) << 3) | r)
#define LSEL(s,r)       ((s) | r | SEL_LDT)

#define USERMODE(c, f)      (ISPL(c) == SEL_UPL)
#define KERNELMODE(c, f)    (ISPL(c) == SEL_KPL)

/*
 * Memory and System segment descriptors
 */

/*
 * System segment descriptor (TSS, LDT)
 * 16 bytes in long mode
 */
struct sys_segment_descriptor {
    uint64_t sd_lolimit:16;     /* segment extent (lsb) */
    uint64_t sd_lobase:24;      /* segment base address (lsb) */
    uint64_t sd_type:5;         /* segment type */
    uint64_t sd_dpl:2;          /* segment descriptor priority level */
    uint64_t sd_p:1;            /* segment descriptor present */
    uint64_t sd_hilimit:4;      /* segment extent (msb) */
    uint64_t sd_xx1:3;          /* avl, long and def32 (not used) */
    uint64_t sd_gran:1;         /* limit granularity (byte/page) */
    uint64_t sd_hibase:40;      /* segment base address (msb) */
    uint64_t sd_xx2:8;          /* reserved */
    uint64_t sd_zero:5;         /* must be zero */
    uint64_t sd_xx3:19;         /* reserved */
} __attribute__((packed));

/*
 * Memory segment descriptor (CS, DS, etc.)
 * 8 bytes
 */
struct mem_segment_descriptor {
    uint32_t sd_lolimit:16;     /* segment extent (lsb) */
    uint32_t sd_lobase:24;      /* segment base address (lsb) */
    uint32_t sd_type:5;         /* segment type */
    uint32_t sd_dpl:2;          /* segment descriptor priority level */
    uint32_t sd_p:1;            /* segment descriptor present */
    uint32_t sd_hilimit:4;      /* segment extent (msb) */
    uint32_t sd_avl:1;          /* available */
    uint32_t sd_long:1;         /* long mode */
    uint32_t sd_def32:1;        /* default 32 vs 16 bit size */
    uint32_t sd_gran:1;         /* limit granularity (byte/page) */
    uint32_t sd_hibase:8;       /* segment base address (msb) */
} __attribute__((packed));

/*
 * Gate descriptor (interrupt/trap gates)
 * 16 bytes in long mode
 */
struct gate_descriptor {
    uint64_t gd_looffset:16;    /* gate offset (lsb) */
    uint64_t gd_selector:16;    /* gate segment selector */
    uint64_t gd_ist:3;          /* IST select */
    uint64_t gd_xx1:5;          /* reserved */
    uint64_t gd_type:5;         /* segment type */
    uint64_t gd_dpl:2;          /* segment descriptor priority level */
    uint64_t gd_p:1;            /* segment descriptor present */
    uint64_t gd_hioffset:48;    /* gate offset (msb) */
    uint64_t gd_xx2:8;          /* reserved */
    uint64_t gd_zero:5;         /* must be zero */
    uint64_t gd_xx3:19;         /* reserved */
} __attribute__((packed));

/*
 * Region descriptor, used to load GDT/IDT tables
 */
struct region_descriptor {
    uint16_t rd_limit;          /* segment extent */
    uint64_t rd_base;           /* base address */
} __attribute__((packed));

/*
 * Helper functions for setting up descriptors
 */
void setgate(struct gate_descriptor *gd, void *func, int ist, int type, int dpl, int selector);
void unsetgate(struct gate_descriptor *gd);
void setregion(struct region_descriptor *rd, void *base, uint16_t limit);
void set_sys_segment(struct sys_segment_descriptor *sd, void *base, uint64_t limit,
                     int type, int dpl, int gran);
void set_mem_segment(struct mem_segment_descriptor *sd, void *base, uint64_t limit,
                     int type, int dpl, int gran, int def32, int is_long);

/* System segments and gate types */
#define SDT_SYSNULL      0      /* system null */
#define SDT_SYS286TSS    1      /* system 286 TSS available */
#define SDT_SYSLDT       2      /* system local descriptor table */
#define SDT_SYS286BSY    3      /* system 286 TSS busy */
#define SDT_SYS286CGT    4      /* system 286 call gate */
#define SDT_SYSTASKGT    5      /* system task gate */
#define SDT_SYS286IGT    6      /* system 286 interrupt gate */
#define SDT_SYS286TGT    7      /* system 286 trap gate */
#define SDT_SYSNULL2     8      /* system null again */
#define SDT_SYS386TSS    9      /* system 386 TSS available */
#define SDT_SYSNULL3    10      /* system null again */
#define SDT_SYS386BSY   11      /* system 386 TSS busy */
#define SDT_SYS386CGT   12      /* system 386 call gate */
#define SDT_SYSNULL4    13      /* system null again */
#define SDT_SYS386IGT   14      /* system 386 interrupt gate */
#define SDT_SYS386TGT   15      /* system 386 trap gate */

/* Memory segment types */
#define SDT_MEMRO       16      /* memory read only */
#define SDT_MEMROA      17      /* memory read only accessed */
#define SDT_MEMRW       18      /* memory read write */
#define SDT_MEMRWA      19      /* memory read write accessed */
#define SDT_MEMROD      20      /* memory read only expand down limit */
#define SDT_MEMRODA     21      /* memory read only expand down limit accessed */
#define SDT_MEMRWD      22      /* memory read write expand down limit */
#define SDT_MEMRWDA     23      /* memory read write expand down limit accessed */
#define SDT_MEME        24      /* memory execute only */
#define SDT_MEMEA       25      /* memory execute only accessed */
#define SDT_MEMER       26      /* memory execute read */
#define SDT_MEMERA      27      /* memory execute read accessed */
#define SDT_MEMEC       28      /* memory execute only conforming */
#define SDT_MEMEAC      29      /* memory execute only accessed conforming */
#define SDT_MEMERC      30      /* memory execute read conforming */
#define SDT_MEMERAC     31      /* memory execute read accessed conforming */

/*
 * Entries in the Interrupt Descriptor Table (IDT)
 */
#define NIDT            256
#define NRSVIDT         32      /* reserved entries for cpu exceptions */

/*
 * Segment Protection Exception code bits
 */
#define SEGEX_EXT       0x01    /* recursive or externally induced */
#define SEGEX_IDT       0x02    /* interrupt descriptor table */
#define SEGEX_TI        0x04    /* local descriptor table */

/*
 * Compatibility with existing UESI GDT definitions
 */
#define GNULL_SEL       0       /* Null descriptor */
#define GCODE_SEL       1       /* Kernel code descriptor */
#define GDATA_SEL       2       /* Kernel data descriptor */
#define GUCODE_SEL      3       /* User code descriptor */
#define GUDATA_SEL      4       /* User data descriptor */
#define GTSS_SEL        5       /* TSS descriptor */

/*
 * Checks for valid user selectors
 */
#define VALID_USER_CSEL(s)  ((s) == GSEL(GUCODE_SEL, SEL_UPL))
#define VALID_USER_DSEL(s)  ((s) == GSEL(GUDATA_SEL, SEL_UPL))

#endif /* _MACHINE_SEGMENTS_H_ */