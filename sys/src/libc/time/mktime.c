#include <time.h>
#include "time_private.h"

time_t
mktime(struct tm *tm)
{
	time_t result;
	int year, month, is_leap;
	long days;
	
	if (tm == NULL)
		return (time_t)-1;
	
	/* Normalize the structure */
	__normalize_tm(tm);
	
	/* Calculate year relative to epoch */
	year = tm->tm_year + EPOCH_YEAR;
	month = tm->tm_mon;
	
	/* Count days since epoch */
	days = 0;
	
	/* Add days for complete years */
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
	
	/* Adjust for timezone (mktime assumes local time) */
	result += timezone;
	
	/* Handle DST */
	/* For now, we don't adjust for DST */
	/* TODO: Implement proper DST handling */
	
	tm->tm_wday = (EPOCH_WDAY + days) % 7;
	if (tm->tm_wday < 0)
		tm->tm_wday += 7;
	
	tm->tm_yday = __mon_yday[is_leap][month] + tm->tm_mday - 1;
	
	return result;
}