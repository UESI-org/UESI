#include <time.h>

clock_t
clock(void)
{
	struct timespec ts;
	
	/* Use CLOCK_PROCESS_CPUTIME_ID for process CPU time */
	/* Fall back to CLOCK_MONOTONIC if not supported */
	if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) != 0) {
		if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
			return (clock_t)-1;
		}
	}
	
	/* Convert to CLOCKS_PER_SEC units */
	/* POSIX requires CLOCKS_PER_SEC, we use 100 (like historic Unix) */
	return (clock_t)((ts.tv_sec * CLOCKS_PER_SEC) + 
	                 (ts.tv_nsec / (1000000000L / CLOCKS_PER_SEC)));
}