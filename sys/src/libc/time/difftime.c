#include <time.h>

double
difftime(time_t time1, time_t time0)
{
	/*
	 * POSIX requires difftime to handle the case where time_t
	 * is not an arithmetic type, but on our system time_t is
	 * a signed 64-bit integer, so simple subtraction works.
	 * 
	 * We cast to double to handle potential overflow gracefully
	 * and to match the POSIX return type.
	 */
	return (double)time1 - (double)time0;
}