#include <time.h>
#include "time_private.h"

static struct tm tm_buf;

struct tm *
gmtime_r(const time_t *timep, struct tm *result)
{
	time_t t;
	int days, rem;
	int year, month;
	int is_leap;
	
	if (timep == NULL || result == NULL)
		return NULL;
	
	t = *timep;
	
	/* Calculate days and remaining seconds since epoch */
	days = t / SECSPERDAY;
	rem = t % SECSPERDAY;
	
	/* Handle negative times */
	if (rem < 0) {
		rem += SECSPERDAY;
		days--;
	}
	
	/* Calculate time of day */
	result->tm_hour = rem / SECSPERHOUR;
	rem %= SECSPERHOUR;
	result->tm_min = rem / SECSPERMIN;
	result->tm_sec = rem % SECSPERMIN;
	
	/* Calculate day of week (Jan 1, 1970 was Thursday = 4) */
	result->tm_wday = (EPOCH_WDAY + days) % DAYSPERWEEK;
	if (result->tm_wday < 0)
		result->tm_wday += DAYSPERWEEK;
	
	/* Calculate year */
	year = POSIX_BASE_YEAR;
	
	/* Fast forward by 400-year cycles (146097 days) */
	while (days < 0 || days >= 146097) {
		int cycles = days / 146097;
		if (days < 0)
			cycles--;
		days -= cycles * 146097;
		year += cycles * 400;
	}
	
	/* Handle remaining years */
	while (1) {
		is_leap = __isleap(year);
		if (days < (is_leap ? DAYSPERLYEAR : DAYSPERNYEAR))
			break;
		days -= is_leap ? DAYSPERLYEAR : DAYSPERNYEAR;
		year++;
	}
	
	result->tm_year = year - EPOCH_YEAR;
	result->tm_yday = days;
	
	/* Find month */
	for (month = 0; month < 11; month++) {
		int mdays = __mon_lengths[is_leap][month];
		if (days < mdays)
			break;
		days -= mdays;
	}
	
	result->tm_mon = month;
	result->tm_mday = days + 1;
	result->tm_isdst = 0; /* No DST in UTC */
	result->tm_gmtoff = 0; /* UTC offset is 0 */
	result->tm_zone = "UTC";
	
	return result;
}

struct tm *
gmtime(const time_t *timep)
{
	return gmtime_r(timep, &tm_buf);
}