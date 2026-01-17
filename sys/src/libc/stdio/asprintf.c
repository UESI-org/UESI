#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include <errno.h>

#if __BSD_VISIBLE

int
vasprintf(char **strp, const char *fmt, va_list ap)
{
	char *str;
	int ret;
	va_list ap_copy;
	
	if (strp == NULL) {
		errno = EINVAL;
		return -1;
	}
	
	va_copy(ap_copy, ap);
	
	ret = vsnprintf(NULL, 0, fmt, ap);
	if (ret < 0) {
		va_end(ap_copy);
		return -1;
	}
	
	str = malloc(ret + 1);
	if (str == NULL) {
		va_end(ap_copy);
		errno = ENOMEM;
		return -1;
	}
	
	ret = vsnprintf(str, ret + 1, fmt, ap_copy);
	va_end(ap_copy);
	
	if (ret < 0) {
		free(str);
		return -1;
	}
	
	*strp = str;
	return ret;
}

int
asprintf(char **strp, const char *fmt, ...)
{
	va_list ap;
	int ret;
	
	va_start(ap, fmt);
	ret = vasprintf(strp, fmt, ap);
	va_end(ap);
	
	return ret;
}

#endif /* __BSD_VISIBLE */