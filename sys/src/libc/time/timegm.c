#include <time.h>
#include "time_private.h"

time_t
timegm(struct tm *tm)
{
	time_t result;
	int year, month, is_leap;
	long days;
	
	if (tm == NULL)
		return (time_t)-1;
	
	__normalize_tm(tm);
	
	year = tm->tm_year + EPOCH_YEAR;
	month = tm->tm_mon;
	
	days = 0;
	
	for (int y = POSIX_BASE_YEAR; y < year; y++) {
		days += __isleap(y) ? 366 : 365;
	}
	
	is_leap = __isleap(year);
	days += __mon_yday[is_leap][month];
	
	days += tm->tm_mday - 1;
	
	result = days * SECSPERDAY;
	result += tm->tm_hour * SECSPERHOUR;
	result += tm->tm_min * SECSPERMIN;
	result += tm->tm_sec;
	
	tm->tm_wday = (EPOCH_WDAY + days) % 7;
	if (tm->tm_wday < 0)
		tm->tm_wday += 7;
	
	tm->tm_yday = __mon_yday[is_leap][month] + tm->tm_mday - 1;
	tm->tm_isdst = 0; /* UTC has no DST */
	
	return result;
}