#include <time.h>
#include "time_private.h"

static struct tm tm_buf;

struct tm *
localtime_r(const time_t *timep, struct tm *result)
{
	time_t t;
	
	if (timep == NULL || result == NULL)
		return NULL;
	
	/* For now, just use UTC (localtime == gmtime) */
	/* TODO: Implement proper timezone support using TZ environment variable */
	t = *timep - timezone;
	
	if (gmtime_r(&t, result) == NULL)
		return NULL;
	
	result->tm_isdst = daylight;
	result->tm_gmtoff = -timezone;
	result->tm_zone = tzname[daylight ? 1 : 0];
	
	return result;
}

struct tm *
localtime(const time_t *timep)
{
	return localtime_r(timep, &tm_buf);
}