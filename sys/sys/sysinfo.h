#ifndef _SYS_SYSINFO_H_
#define _SYS_SYSINFO_H_

#include <sys/types.h>

struct sysinfo {
    int64_t uptime;             /* Seconds since boot */
    uint64_t loads[3];          /* 1, 5, and 15 minute load averages */
    uint64_t totalram;          /* Total usable main memory size */
    uint64_t freeram;           /* Available memory size */
    uint64_t sharedram;         /* Amount of shared memory */
    uint64_t bufferram;         /* Memory used by buffers */
    uint64_t totalswap;         /* Total swap space size */
    uint64_t freeswap;          /* Swap space still available */
    uint16_t procs;             /* Number of current processes */
    uint64_t totalhigh;         /* Total high memory size */
    uint64_t freehigh;          /* Available high memory size */
    uint32_t mem_unit;          /* Memory unit size in bytes */
    char _f[20-2*sizeof(uint64_t)-sizeof(uint32_t)];  /* Padding for alignment */
};

#ifdef _KERNEL
int kern_sysinfo(struct sysinfo *info);
void sysinfo_init(void);
void sysinfo_update_load(void);
#endif

#endif /* !_SYS_SYSINFO_H_ */