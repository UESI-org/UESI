#include <stdlib.h>
#include <sys/cdefs.h>

__dead void
abort(void)
{
	/* Don't call atexit handlers - call _Exit directly */
	_Exit(128 + 6); /* SIGABRT = 6 */
}
