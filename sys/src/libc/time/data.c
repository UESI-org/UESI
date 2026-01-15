#include "time_private.h"

/* Days in each month [is_leap][month] */
const int __mon_lengths[2][12] = {
	{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
	{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

/* Days in year before each month [is_leap][month] */
const int __mon_yday[2][12] = {
	{ 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 },
	{ 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 }
};

char *tzname[2] = { "UTC", "UTC" };

long timezone = 0;

int daylight = 0;