#include <stdlib.h>
#include <errno.h>

#ifndef _KERNEL
#endif

#if __POSIX_VISIBLE >= 199506 || __XPG_VISIBLE
int
system(const char *command)
{
	/* Not implemented in kernel space */
	(void)command;
	errno = ENOSYS;
	return -1;
}
#endif

#if __POSIX_VISIBLE >= 200112
char *
realpath(const char *__restrict path, char *__restrict resolved_path)
{
	/* TODO: Implement with VFS support */
	(void)path;
	(void)resolved_path;
	errno = ENOSYS;
	return NULL;
}
#endif
