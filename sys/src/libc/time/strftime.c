#include <time.h>
#include <string.h>
#include <stdio.h>
#include "time_private.h"

static const char wday_name[7][4] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static const char wday_full[7][10] = {
	"Sunday", "Monday", "Tuesday", "Wednesday",
	"Thursday", "Friday", "Saturday"
};

static const char mon_name[12][4] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static const char mon_full[12][10] = {
	"January", "February", "March", "April", "May", "June",
	"July", "August", "September", "October", "November", "December"
};

size_t
strftime(char *s, size_t maxsize, const char *format, const struct tm *tm)
{
	size_t n = 0;
	char buf[64];
	const char *p;
	char c;
	
	if (s == NULL || format == NULL || tm == NULL || maxsize == 0)
		return 0;
	
	for (p = format; (c = *p) != '\0'; p++) {
		if (c != '%') {
			if (n < maxsize - 1) {
				s[n++] = c;
			} else {
				return 0;
			}
			continue;
		}
		
		/* Format specifier */
		c = *++p;
		if (c == '\0')
			break;
		
		buf[0] = '\0';
		
		switch (c) {
		case 'a': /* Abbreviated weekday name */
			if (tm->tm_wday >= 0 && tm->tm_wday <= 6)
				snprintf(buf, sizeof(buf), "%s", wday_name[tm->tm_wday]);
			break;
		case 'A': /* Full weekday name */
			if (tm->tm_wday >= 0 && tm->tm_wday <= 6)
				snprintf(buf, sizeof(buf), "%s", wday_full[tm->tm_wday]);
			break;
		case 'b': /* Abbreviated month name */
		case 'h':
			if (tm->tm_mon >= 0 && tm->tm_mon <= 11)
				snprintf(buf, sizeof(buf), "%s", mon_name[tm->tm_mon]);
			break;
		case 'B': /* Full month name */
			if (tm->tm_mon >= 0 && tm->tm_mon <= 11)
				snprintf(buf, sizeof(buf), "%s", mon_full[tm->tm_mon]);
			break;
		case 'c': /* Locale's date and time */
			snprintf(buf, sizeof(buf), "%.3s %.3s%3d %.2d:%.2d:%.2d %d",
			         wday_name[tm->tm_wday], mon_name[tm->tm_mon],
			         tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec,
			         tm->tm_year + EPOCH_YEAR);
			break;
		case 'C': /* Century */
			snprintf(buf, sizeof(buf), "%02d", (tm->tm_year + EPOCH_YEAR) / 100);
			break;
		case 'd': /* Day of month [01-31] */
			snprintf(buf, sizeof(buf), "%02d", tm->tm_mday);
			break;
		case 'D': /* MM/DD/YY */
			snprintf(buf, sizeof(buf), "%02d/%02d/%02d",
			         tm->tm_mon + 1, tm->tm_mday, tm->tm_year % 100);
			break;
		case 'e': /* Day of month [ 1-31] */
			snprintf(buf, sizeof(buf), "%2d", tm->tm_mday);
			break;
		case 'F': /* ISO 8601 date (YYYY-MM-DD) */
			snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
			         tm->tm_year + EPOCH_YEAR, tm->tm_mon + 1, tm->tm_mday);
			break;
		case 'H': /* Hour [00-23] */
			snprintf(buf, sizeof(buf), "%02d", tm->tm_hour);
			break;
		case 'I': /* Hour [01-12] */
			snprintf(buf, sizeof(buf), "%02d",
			         (tm->tm_hour % 12) ? (tm->tm_hour % 12) : 12);
			break;
		case 'j': /* Day of year [001-366] */
			snprintf(buf, sizeof(buf), "%03d", tm->tm_yday + 1);
			break;
		case 'm': /* Month [01-12] */
			snprintf(buf, sizeof(buf), "%02d", tm->tm_mon + 1);
			break;
		case 'M': /* Minute [00-59] */
			snprintf(buf, sizeof(buf), "%02d", tm->tm_min);
			break;
		case 'n': /* Newline */
			buf[0] = '\n';
			buf[1] = '\0';
			break;
		case 'p': /* AM/PM */
			snprintf(buf, sizeof(buf), "%s", (tm->tm_hour < 12) ? "AM" : "PM");
			break;
		case 'r': /* 12-hour time with AM/PM */
			snprintf(buf, sizeof(buf), "%02d:%02d:%02d %s",
			         (tm->tm_hour % 12) ? (tm->tm_hour % 12) : 12,
			         tm->tm_min, tm->tm_sec,
			         (tm->tm_hour < 12) ? "AM" : "PM");
			break;
		case 'R': /* HH:MM */
			snprintf(buf, sizeof(buf), "%02d:%02d", tm->tm_hour, tm->tm_min);
			break;
		case 'S': /* Second [00-60] */
			snprintf(buf, sizeof(buf), "%02d", tm->tm_sec);
			break;
		case 't': /* Tab */
			buf[0] = '\t';
			buf[1] = '\0';
			break;
		case 'T': /* HH:MM:SS */
			snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
			         tm->tm_hour, tm->tm_min, tm->tm_sec);
			break;
		case 'u': /* Weekday [1-7], Monday = 1 */
			snprintf(buf, sizeof(buf), "%d", tm->tm_wday ? tm->tm_wday : 7);
			break;
		case 'w': /* Weekday [0-6], Sunday = 0 */
			snprintf(buf, sizeof(buf), "%d", tm->tm_wday);
			break;
		case 'x': /* Locale's date */
			snprintf(buf, sizeof(buf), "%02d/%02d/%02d",
			         tm->tm_mon + 1, tm->tm_mday, tm->tm_year % 100);
			break;
		case 'X': /* Locale's time */
			snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
			         tm->tm_hour, tm->tm_min, tm->tm_sec);
			break;
		case 'y': /* Year [00-99] */
			snprintf(buf, sizeof(buf), "%02d", tm->tm_year % 100);
			break;
		case 'Y': /* Year with century */
			snprintf(buf, sizeof(buf), "%04d", tm->tm_year + EPOCH_YEAR);
			break;
		case 'z': /* Timezone offset */
			if (tm->tm_gmtoff >= 0) {
				snprintf(buf, sizeof(buf), "+%02ld%02ld",
				         tm->tm_gmtoff / 3600,
				         (tm->tm_gmtoff % 3600) / 60);
			} else {
				snprintf(buf, sizeof(buf), "-%02ld%02ld",
				         (-tm->tm_gmtoff) / 3600,
				         ((-tm->tm_gmtoff) % 3600) / 60);
			}
			break;
		case 'Z': /* Timezone name */
			if (tm->tm_zone != NULL)
				snprintf(buf, sizeof(buf), "%s", tm->tm_zone);
			else
				snprintf(buf, sizeof(buf), "UTC");
			break;
		case '%': /* Literal % */
			buf[0] = '%';
			buf[1] = '\0';
			break;
		default:
			/* Unknown format - just skip */
			continue;
		}
		
		/* Copy formatted string to output */
		size_t len = strlen(buf);
		if (n + len >= maxsize) {
			return 0;
		}
		strcpy(s + n, buf);
		n += len;
	}
	
	if (n < maxsize) {
		s[n] = '\0';
		return n;
	}
	
	return 0;
}