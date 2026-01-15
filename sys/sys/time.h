#ifndef _SYS_TIME_H_
#define _SYS_TIME_H_

#include <sys/cdefs.h>
#include <sys/types.h>

#ifndef _TIMEVAL_DECLARED
#define _TIMEVAL_DECLARED

/*
 * Structure returned by gettimeofday(2) system call,
 * and used in other calls.
 */
struct timeval {
	time_t tv_sec;       /* seconds */
	suseconds_t tv_usec; /* and microseconds */
};
#endif

#ifndef _TIMESPEC_DECLARED
#define _TIMESPEC_DECLARED

/*
 * Structure defined by POSIX.1b to be like a timeval.
 */
struct timespec {
	time_t tv_sec; /* seconds */
	long tv_nsec;  /* and nanoseconds */
};
#endif

/*
 * POSIX-compliant conversion macros
 */
#define TIMEVAL_TO_TIMESPEC(tv, ts)                                            \
	do {                                                                   \
		(ts)->tv_sec = (tv)->tv_sec;                                   \
		(ts)->tv_nsec = (tv)->tv_usec * 1000;                          \
	} while (0)

#define TIMESPEC_TO_TIMEVAL(tv, ts)                                            \
	do {                                                                   \
		(tv)->tv_sec = (ts)->tv_sec;                                   \
		(tv)->tv_usec = (ts)->tv_nsec / 1000;                          \
	} while (0)

struct timezone {
	int tz_minuteswest; /* minutes west of Greenwich */
	int tz_dsttime;     /* type of dst correction */
};

#define DST_NONE 0 /* not on dst */
#define DST_USA 1  /* USA style dst */
#define DST_AUST 2 /* Australian style dst */
#define DST_WET 3  /* Western European dst */
#define DST_MET 4  /* Middle European dst */
#define DST_EET 5  /* Eastern European dst */
#define DST_CAN 6  /* Canada */

/* Operations on timevals. */
#define timerclear(tvp) ((tvp)->tv_sec = (tvp)->tv_usec = 0)
#define timerisset(tvp) ((tvp)->tv_sec || (tvp)->tv_usec)
#define timerisvalid(tvp) ((tvp)->tv_usec >= 0 && (tvp)->tv_usec < 1000000)
#define timercmp(tvp, uvp, cmp)                                                \
	(((tvp)->tv_sec == (uvp)->tv_sec) ? ((tvp)->tv_usec cmp(uvp)->tv_usec) \
	                                  : ((tvp)->tv_sec cmp(uvp)->tv_sec))
#define timeradd(tvp, uvp, vvp)                                                \
	do {                                                                   \
		(vvp)->tv_sec = (tvp)->tv_sec + (uvp)->tv_sec;                 \
		(vvp)->tv_usec = (tvp)->tv_usec + (uvp)->tv_usec;              \
		if ((vvp)->tv_usec >= 1000000) {                               \
			(vvp)->tv_sec++;                                       \
			(vvp)->tv_usec -= 1000000;                             \
		}                                                              \
	} while (0)
#define timersub(tvp, uvp, vvp)                                                \
	do {                                                                   \
		(vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;                 \
		(vvp)->tv_usec = (tvp)->tv_usec - (uvp)->tv_usec;              \
		if ((vvp)->tv_usec < 0) {                                      \
			(vvp)->tv_sec--;                                       \
			(vvp)->tv_usec += 1000000;                             \
		}                                                              \
	} while (0)

/* Operations on timespecs. */
#define timespecclear(tsp) ((tsp)->tv_sec = (tsp)->tv_nsec = 0)
#define timespecisset(tsp) ((tsp)->tv_sec || (tsp)->tv_nsec)
#define timespecisvalid(tsp)                                                   \
	((tsp)->tv_nsec >= 0 && (tsp)->tv_nsec < 1000000000L)
#define timespeccmp(tsp, usp, cmp)                                             \
	(((tsp)->tv_sec == (usp)->tv_sec) ? ((tsp)->tv_nsec cmp(usp)->tv_nsec) \
	                                  : ((tsp)->tv_sec cmp(usp)->tv_sec))
#define timespecadd(tsp, usp, vsp)                                             \
	do {                                                                   \
		(vsp)->tv_sec = (tsp)->tv_sec + (usp)->tv_sec;                 \
		(vsp)->tv_nsec = (tsp)->tv_nsec + (usp)->tv_nsec;              \
		if ((vsp)->tv_nsec >= 1000000000L) {                           \
			(vsp)->tv_sec++;                                       \
			(vsp)->tv_nsec -= 1000000000L;                         \
		}                                                              \
	} while (0)
#define timespecsub(tsp, usp, vsp)                                             \
	do {                                                                   \
		(vsp)->tv_sec = (tsp)->tv_sec - (usp)->tv_sec;                 \
		(vsp)->tv_nsec = (tsp)->tv_nsec - (usp)->tv_nsec;              \
		if ((vsp)->tv_nsec < 0) {                                      \
			(vsp)->tv_sec--;                                       \
			(vsp)->tv_nsec += 1000000000L;                         \
		}                                                              \
	} while (0)

/*
 * Names of the interval timers, and structure
 * defining a timer setting.
 */
#define ITIMER_REAL 0
#define ITIMER_VIRTUAL 1
#define ITIMER_PROF 2

struct itimerval {
	struct timeval it_interval; /* timer interval */
	struct timeval it_value;    /* current value */
};

#if __BSD_VISIBLE

/*
 * clock information structure for sysctl({CTL_KERN, KERN_CLOCKRATE})
 */
struct clockinfo {
	int hz;     /* clock frequency */
	int tick;   /* micro-seconds per hz tick */
	int stathz; /* statistics clock frequency */
	int profhz; /* profiling clock frequency */
};
#endif /* __BSD_VISIBLE */

#if defined(_KERNEL)
#include <sys/stdint.h>

void timekeeping_init(void);

/*
 * Time expressed as seconds and fractions of a second.
 * The fraction is represented as a 64-bit fixed point value where
 * the upper 32 bits are the fractional seconds in units of 2^-32 seconds.
 * This provides sub-nanosecond resolution.
 */
struct bintime {
	time_t sec;
	uint64_t frac;
};

/*
 * Comparison and manipulation of bintime structures
 */
#define bintimecmp(btp, ctp, cmp)                                              \
	((btp)->sec == (ctp)->sec ? (btp)->frac cmp(ctp)->frac                 \
	                          : (btp)->sec cmp(ctp)->sec)

static inline void
bintimeaddfrac(const struct bintime *bt, uint64_t x, struct bintime *ct)
{
	ct->sec = bt->sec;
	ct->frac = bt->frac + x;
	if (ct->frac < bt->frac)
		ct->sec++;
}

static inline void
bintimeadd(const struct bintime *bt,
           const struct bintime *ct,
           struct bintime *dt)
{
	dt->sec = bt->sec + ct->sec;
	dt->frac = bt->frac + ct->frac;
	if (dt->frac < bt->frac)
		dt->sec++;
}

static inline void
bintimesub(const struct bintime *bt,
           const struct bintime *ct,
           struct bintime *dt)
{
	dt->sec = bt->sec - ct->sec;
	dt->frac = bt->frac - ct->frac;
	if (dt->frac > bt->frac)
		dt->sec--;
}

/*
 * Background information:
 *
 * When converting between timestamps on parallel timescales of differing
 * resolutions it is historical and scientific practice to round down rather
 * than doing 4/5 rounding.
 *
 *   The date changes at midnight, not at noon.
 *
 *   Even at 15:59:59.999999999 it's not four'o'clock.
 *
 *   time_second ticks after N.999999999 not after N.4999999999
 */

static inline uint32_t
FRAC_TO_NSEC(uint64_t frac)
{
	return ((frac >> 32) * 1000000000ULL) >> 32;
}

static inline void
BINTIME_TO_TIMESPEC(const struct bintime *bt, struct timespec *ts)
{
	ts->tv_sec = bt->sec;
	ts->tv_nsec = FRAC_TO_NSEC(bt->frac);
}

static inline void
TIMESPEC_TO_BINTIME(const struct timespec *ts, struct bintime *bt)
{
	bt->sec = ts->tv_sec;
	/* 18446744073 = int(2^64 / 1000000000) */
	bt->frac = (uint64_t)ts->tv_nsec * (uint64_t)18446744073ULL;
}

static inline void
BINTIME_TO_TIMEVAL(const struct bintime *bt, struct timeval *tv)
{
	tv->tv_sec = bt->sec;
	tv->tv_usec =
	    (long)(((uint64_t)1000000 * (uint32_t)(bt->frac >> 32)) >> 32);
}

static inline void
TIMEVAL_TO_BINTIME(const struct timeval *tv, struct bintime *bt)
{
	bt->sec = (time_t)tv->tv_sec;
	/* 18446744073709 = int(2^64 / 1000000) */
	bt->frac = (uint64_t)tv->tv_usec * (uint64_t)18446744073709ULL;
}

/*
 * Conversion utilities for common time units
 */
static inline void
USEC_TO_TIMEVAL(uint64_t us, struct timeval *tv)
{
	tv->tv_sec = us / 1000000;
	tv->tv_usec = us % 1000000;
}

static inline void
NSEC_TO_TIMEVAL(uint64_t ns, struct timeval *tv)
{
	tv->tv_sec = ns / 1000000000L;
	tv->tv_usec = (ns % 1000000000L) / 1000;
}

static inline uint64_t
TIMEVAL_TO_NSEC(const struct timeval *tv)
{
	uint64_t nsecs;

	/* Handle negative time */
	if (tv->tv_sec < 0)
		return 0;

	/* Check for overflow */
	if ((uint64_t)tv->tv_sec > UINT64_MAX / 1000000000ULL)
		return UINT64_MAX;
	nsecs = (uint64_t)tv->tv_sec * 1000000000ULL;
	if (tv->tv_usec * 1000ULL > UINT64_MAX - nsecs)
		return UINT64_MAX;
	return nsecs + tv->tv_usec * 1000ULL;
}

static inline void
NSEC_TO_TIMESPEC(uint64_t ns, struct timespec *ts)
{
	ts->tv_sec = ns / 1000000000L;
	ts->tv_nsec = ns % 1000000000L;
}

static inline uint64_t
SEC_TO_NSEC(uint64_t seconds)
{
	if (seconds > UINT64_MAX / 1000000000ULL)
		return UINT64_MAX;
	return seconds * 1000000000ULL;
}

static inline uint64_t
MSEC_TO_NSEC(uint64_t milliseconds)
{
	if (milliseconds > UINT64_MAX / 1000000ULL)
		return UINT64_MAX;
	return milliseconds * 1000000ULL;
}

static inline uint64_t
USEC_TO_NSEC(uint64_t microseconds)
{
	if (microseconds > UINT64_MAX / 1000ULL)
		return UINT64_MAX;
	return microseconds * 1000ULL;
}

static inline uint64_t
TIMESPEC_TO_NSEC(const struct timespec *ts)
{
	/* Handle negative time */
	if (ts->tv_sec < 0)
		return 0;

	/* Check for overflow */
	if ((uint64_t)ts->tv_sec > (UINT64_MAX - ts->tv_nsec) / 1000000000ULL)
		return UINT64_MAX;
	return (uint64_t)ts->tv_sec * 1000000000ULL + ts->tv_nsec;
}

static inline uint64_t
BINTIME_TO_NSEC(const struct bintime *bt)
{
	return bt->sec * 1000000000ULL + FRAC_TO_NSEC(bt->frac);
}

/*
 * TSC-based time conversion
 * Note: TSC functions are declared in <tsc.h>, included separately
 */
#ifdef TSC_H
static inline uint64_t
TSC_TO_NSEC(uint64_t tsc)
{
	return tsc_to_ns(tsc);
}

static inline uint64_t
NSEC_TO_TSC(uint64_t nsec)
{
	return ns_to_tsc(nsec);
}
#endif /* TSC_H */

/* Tick conversion using global tick_nsec */
extern int tick_nsec;

static inline uint64_t
TICKS_TO_NSEC(uint64_t ticks)
{
	return ticks * tick_nsec;
}

/*
 * Functions for looking at our clocks: [get]{bin,nano,micro}[boot|up]time()
 *
 * Functions without the "get" prefix return the best timestamp
 * we can produce in the given format.
 *
 * "bin"   == struct bintime  == seconds + 64 bit fraction of seconds.
 * "nano"  == struct timespec == seconds + nanoseconds.
 * "micro" == struct timeval  == seconds + microseconds.
 *
 * Functions containing "up" return time relative to boot and
 * should be used for calculating time intervals.
 *
 * Functions containing "boot" return the GMT time at which the
 * system booted.
 *
 * Functions with just "time" return the current GMT time.
 *
 * Functions with the "get" prefix return a less precise result
 * much faster than the functions without "get" prefix and should
 * be used where a precision of 10 msec is acceptable or where
 * performance is priority. (NB: "precision", _not_ "resolution" !)
 */

void bintime(struct bintime *);
void nanotime(struct timespec *);
void microtime(struct timeval *);

void getnanotime(struct timespec *);
void getmicrotime(struct timeval *);

void binuptime(struct bintime *);
void nanouptime(struct timespec *);
void microuptime(struct timeval *);

void getbinuptime(struct bintime *);
void getnanouptime(struct timespec *);
void getmicrouptime(struct timeval *);

void binboottime(struct bintime *);
void microboottime(struct timeval *);
void nanoboottime(struct timespec *);

void binruntime(struct bintime *);
void nanoruntime(struct timespec *);

void getbinruntime(struct bintime *);
uint64_t getnsecruntime(void);

time_t gettime(void);
time_t getuptime(void);

uint64_t nsecuptime(void);
uint64_t getnsecuptime(void);

struct proc;
#ifdef _KERNEL
int clock_gettime(struct proc *, clockid_t, struct timespec *);
#endif

struct clockrequest;
void itimer_update(struct clockrequest *, void *, void *);

void cancel_all_itimers(void);
int settime(const struct timespec *);
int ratecheck(struct timeval *, const struct timeval *);
int ppsratecheck(struct timeval *, int *, int);

/*
 * "POSIX time" to/from "YY/MM/DD/hh/mm/ss"
 */
struct clock_ymdhms {
	u_short dt_year;
	u_char dt_mon;
	u_char dt_day;
	u_char dt_wday; /* Day of week */
	u_char dt_hour;
	u_char dt_min;
	u_char dt_sec;
};

time_t clock_ymdhms_to_secs(struct clock_ymdhms *);
void clock_secs_to_ymdhms(time_t, struct clock_ymdhms *);

/*
 * BCD to decimal and decimal to BCD.
 */
#define FROMBCD(x) (((x) >> 4) * 10 + ((x) & 0xf))
#define TOBCD(x) (((x) / 10 * 16) + ((x) % 10))

/* Some handy constants. */
#define SECDAY 86400L
#define SECYR (SECDAY * 365)

/* Traditional POSIX base year */
#define POSIX_BASE_YEAR 1970

#endif /* _KERNEL */

#if !defined(_KERNEL)
#include <time.h>

#if __BSD_VISIBLE || __XPG_VISIBLE
__BEGIN_DECLS
#if __BSD_VISIBLE
int adjtime(const struct timeval *, struct timeval *);
int adjfreq(const int64_t *, int64_t *);
#endif
#if __XPG_VISIBLE
int futimes(int, const struct timeval *);
int getitimer(int, struct itimerval *);
int gettimeofday(struct timeval *, struct timezone *);
int setitimer(int, const struct itimerval *, struct itimerval *);
int settimeofday(const struct timeval *, const struct timezone *);
int utimes(const char *, const struct timeval *);
#endif /* __XPG_VISIBLE */
__END_DECLS
#endif /* __BSD_VISIBLE || __XPG_VISIBLE */

#endif /* !_KERNEL */

#endif /* !_SYS_TIME_H_ */
