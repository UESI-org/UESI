#include <time.h>
#include <stdio.h>
#include <string.h>
#include "time_private.h"

static char result_buf[26];

static const char wday_name[7][4] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static const char mon_name[12][4] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

char *
asctime_r(const struct tm *tm, char *buf)
{
	if (tm == NULL || buf == NULL)
		return NULL;
	
	/* Validate tm fields */
	if (tm->tm_wday < 0 || tm->tm_wday > 6 ||
	    tm->tm_mon < 0 || tm->tm_mon > 11 ||
	    tm->tm_mday < 1 || tm->tm_mday > 31 ||
	    tm->tm_hour < 0 || tm->tm_hour > 23 ||
	    tm->tm_min < 0 || tm->tm_min > 59 ||
	    tm->tm_sec < 0 || tm->tm_sec > 60) { /* 60 for leap second */
		return NULL;
	}
	
	/* Format: "Day Mon DD HH:MM:SS YYYY\n" */
	snprintf(buf, 26, "%.3s %.3s%3d %.2d:%.2d:%.2d %04d\n",
         wday_name[tm->tm_wday],
         mon_name[tm->tm_mon],
         tm->tm_mday,
         tm->tm_hour,
         tm->tm_min,
         tm->tm_sec,
         tm->tm_year + EPOCH_YEAR);
	
	return buf;
}

char *
asctime(const struct tm *tm)
{
	return asctime_r(tm, result_buf);
}