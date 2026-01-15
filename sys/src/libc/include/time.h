#ifndef _TIME_H_
#define _TIME_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/_types.h>
#include <sys/_null.h>

#ifndef _TIME_T_DEFINED_
#define _TIME_T_DEFINED_
typedef __time_t time_t;
#endif

#ifndef _CLOCK_T_DEFINED_
#define _CLOCK_T_DEFINED_
typedef __clock_t clock_t;
#endif

#ifndef _SIZE_T_DEFINED_
#define _SIZE_T_DEFINED_
typedef __size_t size_t;
#endif

#if __POSIX_VISIBLE > 0 && __POSIX_VISIBLE < 200112 || __BSD_VISIBLE
/*
 * Frequency of the clock ticks reported by clock() and times().
 * Deprecated - use sysconf(_SC_CLK_TCK) instead.
 */
#define CLK_TCK 100
#endif

#define CLOCKS_PER_SEC 100

#if __POSIX_VISIBLE >= 199309
#ifndef _CLOCKID_T_DEFINED_
#define _CLOCKID_T_DEFINED_
typedef __clockid_t clockid_t;
#endif

#ifndef _TIMER_T_DEFINED_
#define _TIMER_T_DEFINED_
typedef __timer_t timer_t;
#endif

#ifndef _TIMESPEC_DECLARED
#define _TIMESPEC_DECLARED
struct timespec {
	time_t tv_sec;  /* seconds */
	long   tv_nsec; /* and nanoseconds */
};
#endif
#endif /* __POSIX_VISIBLE >= 199309 */

/*
 * Structure returned by gmtime(), localtime(), gmtime_r(), localtime_r().
 * This is the "broken-down" time representation.
 */
struct tm {
	int tm_sec;    /* seconds after the minute [0-60] */
	int tm_min;    /* minutes after the hour [0-59] */
	int tm_hour;   /* hours since midnight [0-23] */
	int tm_mday;   /* day of the month [1-31] */
	int tm_mon;    /* months since January [0-11] */
	int tm_year;   /* years since 1900 */
	int tm_wday;   /* days since Sunday [0-6] */
	int tm_yday;   /* days since January 1 [0-365] */
	int tm_isdst;  /* Daylight Saving Time flag */
	long tm_gmtoff; /* offset from UTC in seconds */
	char *tm_zone;  /* timezone abbreviation */
};

struct itimerspec;

__BEGIN_DECLS

char *asctime(const struct tm *);
clock_t clock(void);
char *ctime(const time_t *);
double difftime(time_t, time_t);
struct tm *gmtime(const time_t *);
struct tm *localtime(const time_t *);
time_t mktime(struct tm *);
size_t strftime(char *__restrict, size_t, const char *__restrict,
    const struct tm *__restrict)
    __attribute__((__bounded__(__string__, 1, 2)));
time_t time(time_t *);

#if __POSIX_VISIBLE >= 200112
struct sigevent;
int timer_create(clockid_t, struct sigevent *__restrict, timer_t *__restrict);
int timer_delete(timer_t);
int timer_getoverrun(timer_t);
int timer_gettime(timer_t, struct itimerspec *);
int timer_settime(timer_t, int, const struct itimerspec *__restrict,
    struct itimerspec *__restrict);
#endif

#if __POSIX_VISIBLE
extern char *tzname[2];
void tzset(void);
#endif

#if __POSIX_VISIBLE >= 199309
int clock_getres(clockid_t, struct timespec *);
int clock_gettime(clockid_t, struct timespec *);
int clock_settime(clockid_t, const struct timespec *);
int nanosleep(const struct timespec *, struct timespec *);
#endif

#if __POSIX_VISIBLE >= 199506
char *asctime_r(const struct tm *__restrict, char *__restrict)
    __attribute__((__bounded__(__minbytes__, 2, 26)));
char *ctime_r(const time_t *, char *)
    __attribute__((__bounded__(__minbytes__, 2, 26)));
struct tm *gmtime_r(const time_t *__restrict, struct tm *__restrict);
struct tm *localtime_r(const time_t *__restrict, struct tm *__restrict);
#endif

#if __XSI_VISIBLE
char *strptime(const char *__restrict, const char *__restrict,
    struct tm *__restrict);
#endif

#if __BSD_VISIBLE
int clock_getcpuclockid(pid_t, clockid_t *);
void tzsetwall(void);
time_t timelocal(struct tm *);
time_t timegm(struct tm *);
time_t timeoff(struct tm *, long);
#endif

#if __POSIX_VISIBLE >= 200809 || defined(_BSD_SOURCE)
extern int daylight;
extern long timezone;
#endif

#define CLOCK_REALTIME           0
#define CLOCK_MONOTONIC          1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define CLOCK_THREAD_CPUTIME_ID  3
#define CLOCK_MONOTONIC_RAW      4
#define CLOCK_REALTIME_COARSE    5
#define CLOCK_MONOTONIC_COARSE   6
#define CLOCK_BOOTTIME           7

#define TIMER_ABSTIME 0x1

__END_DECLS

#endif /* !_TIME_H_ */