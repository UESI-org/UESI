#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#if __POSIX_VISIBLE >= 200809

#define MIN_LINE_SIZE 128

ssize_t
getdelim(char **lineptr, size_t *n, int delim, FILE *stream)
{
	char *ptr, *new_ptr;
	size_t pos = 0;
	int c;
	
	if (lineptr == NULL || n == NULL || stream == NULL) {
		errno = EINVAL;
		return -1;
	}
	
	/* Allocate initial buffer if needed */
	if (*lineptr == NULL || *n == 0) {
		*n = MIN_LINE_SIZE;
		*lineptr = malloc(*n);
		if (*lineptr == NULL) {
			errno = ENOMEM;
			return -1;
		}
	}
	
	ptr = *lineptr;
	
	/* Read until delimiter or EOF */
	while ((c = fgetc(stream)) != EOF) {
		/* Grow buffer if needed */
		if (pos >= *n - 1) {
			size_t new_size = *n * 2;
			new_ptr = realloc(ptr, new_size);
			if (new_ptr == NULL) {
				errno = ENOMEM;
				return -1;
			}
			ptr = new_ptr;
			*lineptr = ptr;
			*n = new_size;
		}
		
		ptr[pos++] = c;
		
		/* Check for delimiter */
		if (c == delim) {
			break;
		}
	}
	
	/* Check for error or EOF with no data */
	if (pos == 0) {
		if (ferror(stream)) {
			return -1;
		}
		if (feof(stream)) {
			return -1;
		}
	}
	
	/* Null-terminate */
	ptr[pos] = '\0';
	
	return (ssize_t)pos;
}

ssize_t
getline(char **lineptr, size_t *n, FILE *stream)
{
	return getdelim(lineptr, n, '\n', stream);
}

#endif /* __POSIX_VISIBLE >= 200809 */