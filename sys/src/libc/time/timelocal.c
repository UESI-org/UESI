#include <time.h>

time_t
timelocal(struct tm *tm)
{
	/* timelocal is just an alias for mktime */
	return mktime(tm);
}