#include <sys/sysinfo.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <string.h>

extern uint64_t pmm_get_total_memory(void);
extern uint64_t pmm_get_free_memory(void);
extern uint64_t get_uptime_sec(void);

typedef struct {
    uint64_t total_tasks;
    uint64_t ready_tasks;
    uint64_t running_tasks;
    uint64_t blocked_tasks;
    uint64_t context_switches;
    uint64_t total_ticks;
} scheduler_stats_t;

extern scheduler_stats_t scheduler_get_stats(void);

/*
 * Exponential moving average load calculation
 * Uses fixed-point arithmetic with 16-bit fractional part (scale 65536)
 * 
 * The formula: load_new = load_old * exp(-interval/period) + active * (1 - exp(-interval/period))
 * 
 * For simplicity, we use approximations:
 * - 1 minute:  exp(-5sec/1min)  ≈ 0.9200 (60224/65536)
 * - 5 minutes: exp(-5sec/5min)  ≈ 0.9836 (64424/65536)
 * - 15 minutes: exp(-5sec/15min) ≈ 0.9945 (65165/65536)
 */

/* Load average state (persistent across calls) */
static struct {
    uint64_t load_1;      /* 1-minute load average (scaled by 65536) */
    uint64_t load_5;      /* 5-minute load average (scaled by 65536) */
    uint64_t load_15;     /* 15-minute load average (scaled by 65536) */
    uint64_t last_update; /* Last update time in ticks */
    int initialized;      /* Has been initialized */
} load_state = {0, 0, 0, 0, 0};

#define EXP_1   60224   /* exp(-5/60)   ≈ 0.9200 */
#define EXP_5   64424   /* exp(-5/300)  ≈ 0.9836 */
#define EXP_15  65165   /* exp(-5/900)  ≈ 0.9945 */
#define SCALE   65536   /* Fixed-point scale */

static void calculate_load_averages(void) {
    scheduler_stats_t stats = scheduler_get_stats();
    uint64_t current_ticks = get_uptime_ticks();
    
    /* Get current number of active tasks (running + ready) */
    uint64_t active = stats.ready_tasks + stats.running_tasks;
    uint64_t active_scaled = active << 16;  /* Scale by 65536 */
    
    /* Initialize if first call */
    if (!load_state.initialized) {
        load_state.load_1 = active_scaled;
        load_state.load_5 = active_scaled;
        load_state.load_15 = active_scaled;
        load_state.last_update = current_ticks;
        load_state.initialized = 1;
        return;
    }
    
    /* Only update if at least 5 seconds (500 ticks at 100Hz) have passed */
    uint64_t ticks_elapsed = current_ticks - load_state.last_update;
    if (ticks_elapsed < 500) {
        return;  /* Too soon, don't update */
    }
    
    /* Calculate number of 5-second intervals that passed */
    uint64_t intervals = ticks_elapsed / 500;
    
    /* Apply exponential moving average for each interval */
    for (uint64_t i = 0; i < intervals; i++) {
        /* load_new = load_old * exp_factor + active * (1 - exp_factor) */
        /* Simplified: load_new = (load_old * exp + active * (SCALE - exp)) / SCALE */
        
        load_state.load_1 = (load_state.load_1 * EXP_1 + 
                             active_scaled * (SCALE - EXP_1)) / SCALE;
        
        load_state.load_5 = (load_state.load_5 * EXP_5 + 
                             active_scaled * (SCALE - EXP_5)) / SCALE;
        
        load_state.load_15 = (load_state.load_15 * EXP_15 + 
                              active_scaled * (SCALE - EXP_15)) / SCALE;
    }
    
    load_state.last_update = current_ticks;
}

static void get_load_averages(uint64_t loads[3]) {
    /* Update load averages based on current state */
    calculate_load_averages();
    
    loads[0] = load_state.load_1;
    loads[1] = load_state.load_5;
    loads[2] = load_state.load_15;
}

static uint32_t calculate_mem_usage_percent(uint64_t total, uint64_t free) {
    if (total == 0) return 0;
    uint64_t used = total - free;
    return (uint32_t)((used * 100) / total);
}

int kern_sysinfo(struct sysinfo *info) {
    if (info == NULL) {
        return -1;
    }

    /* Zero out the structure */
    memset(info, 0, sizeof(struct sysinfo));

    /* Get system uptime in seconds */
    info->uptime = (int64_t)get_uptime_sec();

    /* Get memory information from PMM */
    info->totalram = pmm_get_total_memory();
    info->freeram = pmm_get_free_memory();

    /* Get process count from scheduler */
    scheduler_stats_t sched_stats = scheduler_get_stats();
    info->procs = (uint16_t)sched_stats.total_tasks;

    /* Get exponential moving average load */
    get_load_averages(info->loads);

    /* Memory unit size (always 1 byte on x86-64) */
    info->mem_unit = 1;

    /* Fields not implemented yet (set to 0) */
    info->sharedram = 0;    /* Shared memory not implemented */
    info->bufferram = 0;    /* Buffer cache not implemented */
    info->totalswap = 0;    /* No swap space */
    info->freeswap = 0;
    info->totalhigh = 0;    /* High memory is a 32-bit concept */
    info->freehigh = 0;

    return 0;
}

void sysinfo_init(void) {
    load_state.load_1 = 0;
    load_state.load_5 = 0;
    load_state.load_15 = 0;
    load_state.last_update = 0;
    load_state.initialized = 0;
}

void sysinfo_update_load(void) {
    calculate_load_averages();
}