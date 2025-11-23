#include <sys/kernel.h>
#include <sys/param.h>
#include <sys/types.h>

extern void timer_init(uint32_t frequency_hz);
extern uint64_t timer_get_uptime_ms(void);
extern uint64_t timer_get_uptime_sec(void);
extern uint32_t timer_get_frequency(void);

static inline void *memcpy_local(void *dest, const void *src, size_t n) {
    unsigned char *d = dest;
    const unsigned char *s = src;
    while (n--) *d++ = *s++;
    return dest;
}

/* System identification variables */
int hostid = 0;
char hostname[MAXHOSTNAMELEN] = "localhost";
int hostnamelen = 9;  /* strlen("localhost") */
char domainname[MAXHOSTNAMELEN] = "";
int domainnamelen = 0;

/* Time and clock parameters */
int utc_offset = 0;             /* seconds east of UTC (0 = UTC) */
int tick = 10000;               /* microseconds per tick (10000 = 10ms at 100Hz) */
int tick_nsec = 10000000;       /* nanoseconds per tick (10ms at 100Hz) */
volatile int ticks = 0;         /* number of hardclock ticks */
int hz = HZ;                    /* system clock frequency */
int stathz = 0;                 /* statistics clock (not implemented yet) */
int profhz = 0;                 /* profiling clock (not implemented yet) */

void hardclock_init(void) {
    /* Initialize timer with the configured frequency */
    timer_init(hz);
    
    /* Calculate tick durations based on hz */
    tick = 1000000 / hz;        /* microseconds per tick */
    tick_nsec = 1000000000 / hz; /* nanoseconds per tick */
    
    /* Reset tick counter */
    ticks = 0;
}

void hardclock(void) {
    ticks++;
}

uint32_t get_timer_frequency(void) {
    return hz;
}

void statclock_init(void) {
    /* TODO: Not implemented yet - would use separate timer */
    stathz = 0;
    profhz = 0;
}

uint64_t get_uptime_ms(void) {
    return timer_get_uptime_ms();
}

uint64_t get_uptime_sec(void) {
    return timer_get_uptime_sec();
}

uint64_t get_uptime_ticks(void) {
    return (uint64_t)ticks;
}

int sethostname(const char *name, size_t len) {
    if (name == NULL || len == 0 || len >= MAXHOSTNAMELEN) {
        return -1;
    }
    
    memcpy_local(hostname, name, len);
    hostname[len] = '\0';
    hostnamelen = len;
    
    return 0;
}

int gethostname(char *name, size_t len) {
    if (name == NULL || len == 0) {
        return -1;
    }
    
    size_t copylen = hostnamelen < len ? hostnamelen : len - 1;
    memcpy_local(name, hostname, copylen);
    name[copylen] = '\0';
    
    return 0;
}

int setdomainname(const char *name, size_t len) {
    if (name == NULL || len >= MAXHOSTNAMELEN) {
        return -1;
    }
    
    if (len == 0) {
        domainname[0] = '\0';
        domainnamelen = 0;
        return 0;
    }
    
    memcpy_local(domainname, name, len);
    domainname[len] = '\0';
    domainnamelen = len;
    
    return 0;
}

int getdomainname(char *name, size_t len) {
    if (name == NULL || len == 0) {
        return -1;
    }
    
    size_t copylen = domainnamelen < len ? domainnamelen : len - 1;
    memcpy_local(name, domainname, copylen);
    name[copylen] = '\0';
    
    return 0;
}

void sethostid(int id) {
    hostid = id;
}

int gethostid(void) {
    return hostid;
}