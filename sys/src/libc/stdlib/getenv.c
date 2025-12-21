#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifndef _KERNEL
#endif

static char *empty_env[] = { NULL };
static char **environ = empty_env;

char *
getenv(const char *name)
{
	if (name == NULL || *name == '\0' || strchr(name, '=') != NULL) {
		return NULL;
	}

	size_t len = strlen(name);
	for (char **ep = environ; *ep != NULL; ep++) {
		if (strncmp(*ep, name, len) == 0 && (*ep)[len] == '=') {
			return &(*ep)[len + 1];
		}
	}
	return NULL;
}

#if __POSIX_VISIBLE >= 200112
int
setenv(const char *name, const char *value, int overwrite)
{
	/* TODO: Implement properly with dynamic environment */
	(void)name;
	(void)value;
	(void)overwrite;
	errno = ENOMEM;
	return -1;
}

int
unsetenv(const char *name)
{
	/* TODO: Implement properly */
	(void)name;
	errno = EINVAL;
	return -1;
}

int
putenv(char *string)
{
	/* TODO: Implement properly */
	(void)string;
	errno = ENOMEM;
	return -1;
}
#endif
