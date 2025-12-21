#include <stdlib.h>

#if __BSD_VISIBLE

static const char *progname = "unknown";

const char *
getprogname(void)
{
	return progname;
}

void
setprogname(const char *name)
{
	progname = name;
}

#endif /* __BSD_VISIBLE */
