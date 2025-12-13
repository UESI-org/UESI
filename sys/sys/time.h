#ifndef _SYS_TIME_H_
#define _SYS_TIME_H_

#include <sys/types.h>
#include <stdint.h>

/* Time value structure */
struct timeval {
	time_t tv_sec; /* seconds */
	long tv_usec;  /* microseconds */
};

/* High-resolution time structure */
struct timespec {
	time_t tv_sec; /* seconds */
	long tv_nsec;  /* nanoseconds */
};

/* Timezone structure */
struct timezone {
	int tz_minuteswest; /* minutes west of Greenwich */
	int tz_dsttime;     /* type of DST correction */
};

/* Operations on timespec */
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

#define timespeccmp(tvp, uvp, cmp)                                             \
	(((tvp)->tv_sec == (uvp)->tv_sec) ? ((tvp)->tv_nsec cmp(uvp)->tv_nsec) \
	                                  : ((tvp)->tv_sec cmp(uvp)->tv_sec))

#define timespecisset(tvp) ((tvp)->tv_sec || (tvp)->tv_nsec)
#define timespecclear(tvp) ((tvp)->tv_sec = (tvp)->tv_nsec = 0)

/* Operations on timeval */
#define timerclear(tvp) ((tvp)->tv_sec = (tvp)->tv_usec = 0)
#define timerisset(tvp) ((tvp)->tv_sec || (tvp)->tv_usec)
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

/* Convert between timespec and timeval */
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

#endif /* !_SYS_TIME_H_ */