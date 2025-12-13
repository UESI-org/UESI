#ifndef _SYS_KERNEL_H_
#define _SYS_KERNEL_H_

#include <sys/types.h>
#include <sys/param.h>

extern int hostid;
extern char hostname[MAXHOSTNAMELEN];
extern int hostnamelen;
extern char domainname[MAXHOSTNAMELEN];
extern int domainnamelen;

extern int utc_offset;     /* seconds east of UTC */
extern int tick;           /* microseconds per tick (1000000 / hz) */
extern int tick_nsec;      /* nanoseconds per tick (1000000000 / hz) */
extern volatile int ticks; /* number of hardclock ticks since boot */
extern int hz;             /* system clock frequency (ticks per second) */
extern int stathz;         /* statistics clock frequency */
extern int profhz;         /* profiling clock frequency */

#ifndef HZ
#define HZ 100 /* 100 Hz = 10ms tick interval */
#endif

#define TICK tick /* microseconds per tick */
#define TICK_NSEC tick_nsec

#define tick_to_ms(t) (((t) * 1000) / hz)
#define tick_to_us(t) (((t) * 1000000) / hz)
#define tick_to_ns(t) (((t) * 1000000000ULL) / hz)

#define ms_to_tick(ms) (((ms) * hz) / 1000)
#define us_to_tick(us) (((us) * hz) / 1000000)
#define ns_to_tick(ns) (((ns) * hz) / 1000000000ULL)

extern uint64_t get_uptime_ms(void);
extern uint64_t get_uptime_sec(void);
extern uint64_t get_uptime_ticks(void);

extern void hardclock_init(void);
extern void hardclock(void);
extern void statclock_init(void);

extern int sethostname(const char *name, size_t len);
extern int gethostname(char *name, size_t len);
extern int setdomainname(const char *name, size_t len);
extern int getdomainname(char *name, size_t len);
extern void sethostid(int id);
extern int gethostid(void);

#endif