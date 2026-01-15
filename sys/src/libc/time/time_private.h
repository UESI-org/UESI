/* Internal time library definitions */

#ifndef _TIME_PRIVATE_H_
#define _TIME_PRIVATE_H_

#include <time.h>

extern const int __mon_lengths[2][12];

extern const int __mon_yday[2][12];

extern char *tzname[2];
extern long timezone;
extern int daylight;

int __isleap(int year);
int __daysinmonth(int month, int year);
time_t __tm_to_time(const struct tm *tm);
void __time_to_tm(time_t t, struct tm *tm, int local);
void __normalize_tm(struct tm *tm);

#define EPOCH_YEAR 1900
#define POSIX_BASE_YEAR 1970
#define SECSPERMIN 60
#define MINSPERHOUR 60
#define HOURSPERDAY 24
#define DAYSPERWEEK 7
#define DAYSPERNYEAR 365
#define DAYSPERLYEAR 366
#define SECSPERHOUR (SECSPERMIN * MINSPERHOUR)
#define SECSPERDAY (SECSPERHOUR * HOURSPERDAY)
#define MONSPERYEAR 12

#define EPOCH_WDAY 4

#endif /* _TIME_PRIVATE_H_ */