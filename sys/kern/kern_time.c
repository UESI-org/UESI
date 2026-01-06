/* keep heavily commented! */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/kernel.h>

#include <tsc.h>
#include <rtc.h>

static struct bintime boottime_bt;	/* Boot time in bintime format */
static uint64_t tsc_at_boot;		/* TSC value at boot */
static int timekeeping_initialized = 0;

void
timekeeping_init(void)
{
	rtc_time_t rtc_time;
	time_t boot_timestamp;
	
	if (timekeeping_initialized)
		return;
	
	/* Get current wall clock time from RTC */
	if (rtc_get_time(&rtc_time) == 0) {
		/* Convert to Unix timestamp */
		boot_timestamp = rtc_get_timestamp();
		boottime_bt.sec = boot_timestamp;
		boottime_bt.frac = 0;
	} else {
		/* Fallback: assume epoch if RTC unavailable */
		boottime_bt.sec = 0;
		boottime_bt.frac = 0;
	}
	
	/* Record TSC at boot */
	tsc_at_boot = tsc_read();
	
	timekeeping_initialized = 1;
}

/*
 * Get uptime as bintime (time since boot)
 */
void
binuptime(struct bintime *bt)
{
	uint64_t tsc_now, tsc_delta, ns;
	
	tsc_now = tsc_read();
	tsc_delta = tsc_now - tsc_at_boot;
	ns = tsc_to_ns(tsc_delta);
	
	bt->sec = ns / 1000000000ULL;
	/* Convert remaining nanoseconds to fraction */
	bt->frac = ((ns % 1000000000ULL) * 18446744073ULL);
}

/*
 * Get uptime as timespec (time since boot)
 */
void
nanouptime(struct timespec *ts)
{
	uint64_t ns;
	
	ns = tsc_to_ns(tsc_read() - tsc_at_boot);
	NSEC_TO_TIMESPEC(ns, ts);
}

/*
 * Get uptime as timeval (time since boot)
 */
void
microuptime(struct timeval *tv)
{
	uint64_t ns;
	
	ns = tsc_to_ns(tsc_read() - tsc_at_boot);
	NSEC_TO_TIMEVAL(ns, tv);
}

/*
 * Fast versions using less precise timing
 * (TSC is already fast, so these are the same as the precise versions)
 */
void
getbinuptime(struct bintime *bt)
{
	binuptime(bt);
}

void
getnanouptime(struct timespec *ts)
{
	nanouptime(ts);
}

void
getmicrouptime(struct timeval *tv)
{
	microuptime(tv);
}

/*
 * Get current absolute time (wall clock time)
 */
void
bintime(struct bintime *bt)
{
	struct bintime uptime;
	
	binuptime(&uptime);
	bintimeadd(&boottime_bt, &uptime, bt);
}

void
nanotime(struct timespec *ts)
{
	struct bintime bt;
	
	bintime(&bt);
	BINTIME_TO_TIMESPEC(&bt, ts);
}

void
microtime(struct timeval *tv)
{
	struct bintime bt;
	
	bintime(&bt);
	BINTIME_TO_TIMEVAL(&bt, tv);
}

/*
 * Fast versions of absolute time
 * (TSC is already fast, so these are the same as the precise versions)
 */
void
getnanotime(struct timespec *ts)
{
	nanotime(ts);
}

void
getmicrotime(struct timeval *tv)
{
	microtime(tv);
}

/*
 * Get boot time
 */
void
binboottime(struct bintime *bt)
{
	*bt = boottime_bt;
}

void
nanoboottime(struct timespec *ts)
{
	BINTIME_TO_TIMESPEC(&boottime_bt, ts);
}

void
microboottime(struct timeval *tv)
{
	BINTIME_TO_TIMEVAL(&boottime_bt, tv);
}

/*
 * Runtime is the same as uptime in this implementation
 * (could be different if we support suspend/resume)
 */
void
binruntime(struct bintime *bt)
{
	binuptime(bt);
}

void
nanoruntime(struct timespec *ts)
{
	nanouptime(ts);
}

void
getbinruntime(struct bintime *bt)
{
	getbinuptime(bt);
}

uint64_t
getnsecruntime(void)
{
	return tsc_to_ns(tsc_read() - tsc_at_boot);
}

/*
 * Simple time getters
 */
time_t
gettime(void)
{
	struct bintime bt;
	bintime(&bt);
	return bt.sec;
}

time_t
getuptime(void)
{
	struct bintime bt;
	binuptime(&bt);
	return bt.sec;
}

uint64_t
nsecuptime(void)
{
	return tsc_to_ns(tsc_read() - tsc_at_boot);
}

uint64_t
getnsecuptime(void)
{
	return nsecuptime();
}

/*
 * Set system time
 */
int
settime(const struct timespec *ts)
{
	struct bintime bt, uptime;
	
	if (!timespecisvalid(ts))
		return -1;
	
	/* Convert to bintime */
	TIMESPEC_TO_BINTIME(ts, &bt);
	
	/* Calculate new boot time = current_time - uptime */
	binuptime(&uptime);
	bintimesub(&bt, &uptime, &boottime_bt);
	
	/* TODO: Update RTC hardware if available */
	/* This would require converting ts->tv_sec back to rtc_time_t */
	
	return 0;
}

/*
 * Rate limiting check
 */
int
ratecheck(struct timeval *lasttime, const struct timeval *mininterval)
{
	struct timeval tv, delta;
	
	microtime(&tv);
	timersub(&tv, lasttime, &delta);
	
	if (timercmp(&delta, mininterval, <))
		return 0;
	
	*lasttime = tv;
	return 1;
}

int
ppsratecheck(struct timeval *lasttime, int *curpps, int maxpps)
{
	struct timeval tv, delta;
	int rv = 0;
	
	microtime(&tv);
	timersub(&tv, lasttime, &delta);
	
	if (delta.tv_sec == 0) {
		(*curpps)++;
		if (*curpps <= maxpps)
			rv = 1;
	} else {
		*curpps = 0;
		*lasttime = tv;
		rv = 1;
	}
	
	return rv;
}

/*
 * Convert POSIX time to date/time components
 */
void
clock_secs_to_ymdhms(time_t secs, struct clock_ymdhms *dt)
{
	int days, year, month;
	const int daysperyear[2] = { 365, 366 };
	const int daysinmonth[2][12] = {
		{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
		{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
	};
	
	/* Seconds in day */
	dt->dt_sec = secs % 60;
	secs /= 60;
	dt->dt_min = secs % 60;
	secs /= 60;
	dt->dt_hour = secs % 24;
	days = secs / 24;
	
	/* Day of week (Jan 1, 1970 was a Thursday = 4) */
	dt->dt_wday = (days + 4) % 7;
	
	/* Year */
	year = POSIX_BASE_YEAR;
	while (1) {
		int isleap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
		int yeardays = daysperyear[isleap];
		
		if (days < yeardays)
			break;
		
		days -= yeardays;
		year++;
	}
	dt->dt_year = year;
	
	/* Month and day */
	int isleap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
	for (month = 0; month < 12; month++) {
		if (days < daysinmonth[isleap][month])
			break;
		days -= daysinmonth[isleap][month];
	}
	dt->dt_mon = month + 1;
	dt->dt_day = days + 1;
}

/*
 * Convert date/time components to POSIX time
 */
time_t
clock_ymdhms_to_secs(struct clock_ymdhms *dt)
{
	time_t secs = 0;
	int year, month;
	const int daysinmonth[2][12] = {
		{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
		{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
	};
	
	/* Count years since epoch */
	for (year = POSIX_BASE_YEAR; year < dt->dt_year; year++) {
		int isleap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
		secs += (isleap ? 366 : 365) * SECDAY;
	}
	
	/* Count months in current year */
	int isleap = (dt->dt_year % 4 == 0 && 
	              (dt->dt_year % 100 != 0 || dt->dt_year % 400 == 0));
	for (month = 0; month < dt->dt_mon - 1; month++) {
		secs += daysinmonth[isleap][month] * SECDAY;
	}
	
	/* Add remaining days, hours, minutes, seconds */
	secs += (dt->dt_day - 1) * SECDAY;
	secs += dt->dt_hour * 3600;
	secs += dt->dt_min * 60;
	secs += dt->dt_sec;
	
	return secs;
}

/*
 * Clock gettime implementation for processes
 * This would be called from system call handler
 */
int
clock_gettime(struct proc *p, clockid_t clock_id, struct timespec *ts)
{
	(void)p;	/* Unused for now */
	
	switch (clock_id) {
	case 0:	/* CLOCK_REALTIME */
		nanotime(ts);
		break;
	case 1:	/* CLOCK_MONOTONIC */
		nanouptime(ts);
		break;
	case 4:	/* CLOCK_MONOTONIC_RAW */
		nanouptime(ts);
		break;
	case 7:	/* CLOCK_BOOTTIME */
		nanouptime(ts);
		break;
	default:
		return -1;
	}
	
	return 0;
}

/*
 * Interval timer management stubs
 * These would need full implementation for itimer support
 */
void
itimer_update(struct clockrequest *cr, void *arg1, void *arg2)
{
	(void)cr;
	(void)arg1;
	(void)arg2;
	/* TODO: Implement interval timer updates */
}

void
cancel_all_itimers(void)
{
	/* TODO: Implement interval timer cancellation */
}
