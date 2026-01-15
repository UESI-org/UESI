#include <time.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include "time_private.h"

static const char * const wday_name[] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static const char * const mon_name[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static int
match_string(const char **buf, const char *const *strs, int nstrs)
{
	int i;
	
	for (i = 0; i < nstrs; i++) {
		size_t len = strlen(strs[i]);
		if (strncasecmp(*buf, strs[i], len) == 0) {
			*buf += len;
			return i;
		}
	}
	return -1;
}

static int
parse_num(const char **buf, int min, int max, int width)
{
	int val = 0;
	int ndig = 0;
	const char *p = *buf;
	
	while (width > 0 && isdigit((unsigned char)*p)) {
		val = val * 10 + (*p - '0');
		p++;
		width--;
		ndig++;
	}
	
	if (ndig == 0 || val < min || val > max)
		return -1;
	
	*buf = p;
	return val;
}

char *
strptime(const char *buf, const char *format, struct tm *tm)
{
	const char *p, *bp;
	int val;
	char c;
	
	if (buf == NULL || format == NULL || tm == NULL)
		return NULL;
	
	bp = buf;
	
	for (p = format; (c = *p) != '\0'; p++) {
		if (isspace((unsigned char)c)) {
			/* Match whitespace */
			while (isspace((unsigned char)*bp))
				bp++;
			continue;
		}
		
		if (c != '%') {
			/* Literal character */
			if (*bp != c)
				return NULL;
			bp++;
			continue;
		}
		
		/* Format specifier */
		c = *++p;
		if (c == '\0')
			break;
		
		switch (c) {
		case 'a': /* Abbreviated weekday name */
		case 'A': /* Full weekday name */
			val = match_string(&bp, wday_name, 7);
			if (val < 0)
				return NULL;
			tm->tm_wday = val;
			break;
			
		case 'b': /* Abbreviated month name */
		case 'B': /* Full month name */
		case 'h':
			val = match_string(&bp, mon_name, 12);
			if (val < 0)
				return NULL;
			tm->tm_mon = val;
			break;
			
		case 'd': /* Day of month [01-31] */
		case 'e': /* Day of month [ 1-31] */
			val = parse_num(&bp, 1, 31, 2);
			if (val < 0)
				return NULL;
			tm->tm_mday = val;
			break;
			
		case 'H': /* Hour [00-23] */
			val = parse_num(&bp, 0, 23, 2);
			if (val < 0)
				return NULL;
			tm->tm_hour = val;
			break;
			
		case 'I': /* Hour [01-12] */
			val = parse_num(&bp, 1, 12, 2);
			if (val < 0)
				return NULL;
			tm->tm_hour = val % 12; /* Will be adjusted by %p if present */
			break;
			
		case 'm': /* Month [01-12] */
			val = parse_num(&bp, 1, 12, 2);
			if (val < 0)
				return NULL;
			tm->tm_mon = val - 1;
			break;
			
		case 'M': /* Minute [00-59] */
			val = parse_num(&bp, 0, 59, 2);
			if (val < 0)
				return NULL;
			tm->tm_min = val;
			break;
			
		case 'p': /* AM/PM */
			if (strncasecmp(bp, "AM", 2) == 0) {
				/* AM - hour is already correct if < 12 */
				bp += 2;
			} else if (strncasecmp(bp, "PM", 2) == 0) {
				/* PM - add 12 to hour if < 12 */
				if (tm->tm_hour < 12)
					tm->tm_hour += 12;
				bp += 2;
			} else {
				return NULL;
			}
			break;
			
		case 'S': /* Second [00-60] */
			val = parse_num(&bp, 0, 60, 2);
			if (val < 0)
				return NULL;
			tm->tm_sec = val;
			break;
			
		case 'y': /* Year [00-99] */
			val = parse_num(&bp, 0, 99, 2);
			if (val < 0)
				return NULL;
			/* 00-68 -> 2000-2068, 69-99 -> 1969-1999 */
			tm->tm_year = (val <= 68 ? 100 : 0) + val;
			break;
			
		case 'Y': /* Year with century */
			val = parse_num(&bp, 0, 9999, 4);
			if (val < 0)
				return NULL;
			tm->tm_year = val - EPOCH_YEAR;
			break;
			
		case '%': /* Literal % */
			if (*bp != '%')
				return NULL;
			bp++;
			break;
			
		default:
			/* Unknown format - skip */
			return NULL;
		}
	}
	
	return (char *)bp;
}