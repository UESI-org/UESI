#include "time_private.h"

int
__isleap(int year)
{
	return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

int
__daysinmonth(int month, int year)
{
	if (month < 0 || month > 11)
		return 0;
	
	return __mon_lengths[__isleap(year)][month];
}

/* Normalize tm structure (handle overflow/underflow) */
void
__normalize_tm(struct tm *tm)
{
	int days_in_mon;
	
	/* Normalize seconds */
	while (tm->tm_sec < 0) {
		tm->tm_sec += 60;
		tm->tm_min--;
	}
	while (tm->tm_sec >= 60) {
		tm->tm_sec -= 60;
		tm->tm_min++;
	}
	
	/* Normalize minutes */
	while (tm->tm_min < 0) {
		tm->tm_min += 60;
		tm->tm_hour--;
	}
	while (tm->tm_min >= 60) {
		tm->tm_min -= 60;
		tm->tm_hour++;
	}
	
	/* Normalize hours */
	while (tm->tm_hour < 0) {
		tm->tm_hour += 24;
		tm->tm_mday--;
	}
	while (tm->tm_hour >= 24) {
		tm->tm_hour -= 24;
		tm->tm_mday++;
	}
	
	/* Normalize months */
	while (tm->tm_mon < 0) {
		tm->tm_mon += 12;
		tm->tm_year--;
	}
	while (tm->tm_mon >= 12) {
		tm->tm_mon -= 12;
		tm->tm_year++;
	}
	
	/* Normalize days */
	while (tm->tm_mday < 1) {
		tm->tm_mon--;
		if (tm->tm_mon < 0) {
			tm->tm_mon = 11;
			tm->tm_year--;
		}
		days_in_mon = __daysinmonth(tm->tm_mon, tm->tm_year + EPOCH_YEAR);
		tm->tm_mday += days_in_mon;
	}
	
	while (1) {
		days_in_mon = __daysinmonth(tm->tm_mon, tm->tm_year + EPOCH_YEAR);
		if (tm->tm_mday <= days_in_mon)
			break;
		
		tm->tm_mday -= days_in_mon;
		tm->tm_mon++;
		if (tm->tm_mon >= 12) {
			tm->tm_mon = 0;
			tm->tm_year++;
		}
	}
	
	/* Calculate day of year */
	tm->tm_yday = __mon_yday[__isleap(tm->tm_year + EPOCH_YEAR)][tm->tm_mon] +
	              tm->tm_mday - 1;
	
	/* Calculate day of week */
	/* January 1, 1900 was Monday (1) */
	int days = 0;
	for (int y = 0; y < tm->tm_year; y++) {
		days += __isleap(y + EPOCH_YEAR) ? 366 : 365;
	}
	days += tm->tm_yday;
	tm->tm_wday = (days + 1) % 7; /* +1 because 1900-01-01 was Monday */
}