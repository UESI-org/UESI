#include <time.h>

static char result_buf[26];

char *
ctime_r(const time_t *timep, char *buf)
{
	struct tm tm;
	
	if (timep == NULL || buf == NULL)
		return NULL;
	
	if (localtime_r(timep, &tm) == NULL)
		return NULL;
	
	return asctime_r(&tm, buf);
}

char *
ctime(const time_t *timep)
{
	return ctime_r(timep, result_buf);
}