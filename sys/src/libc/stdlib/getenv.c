#include <sys/malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define ENV_INITIAL_SIZE 32
#define ENV_GROW_FACTOR 2

/* Global environment */
static char **environ = NULL;
static size_t environ_size = 0;
static size_t environ_count = 0;
static int environ_allocated = 0;

/* Get pointer to environ for external access */
char **
__get_environ(void)
{
	return environ;
}

/* Initialize environment if needed */
static int
init_environ(void)
{
	if (environ_allocated) {
		return 0;
	}

	environ = (char **)malloc(ENV_INITIAL_SIZE * sizeof(char *));
	if (!environ) {
		errno = ENOMEM;
		return -1;
	}

	environ[0] = NULL;
	environ_size = ENV_INITIAL_SIZE;
	environ_count = 0;
	environ_allocated = 1;

	return 0;
}

/* Grow environment array */
static int
grow_environ(void)
{
	size_t new_size = environ_size * ENV_GROW_FACTOR;
	char **new_environ = (char **)realloc(environ, new_size * sizeof(char *));
	
	if (!new_environ) {
		errno = ENOMEM;
		return -1;
	}

	environ = new_environ;
	environ_size = new_size;
	return 0;
}

/* Find environment variable by name */
static int
find_env(const char *name, size_t len)
{
	if (!environ) {
		return -1;
	}

	for (size_t i = 0; i < environ_count; i++) {
		if (environ[i] && strncmp(environ[i], name, len) == 0 && 
		    environ[i][len] == '=') {
			return i;
		}
	}

	return -1;
}

char *
getenv(const char *name)
{
	if (!name || *name == '\0' || strchr(name, '=')) {
		return NULL;
	}

	size_t len = strlen(name);
	int idx = find_env(name, len);

	if (idx >= 0) {
		return environ[idx] + len + 1;
	}

	return NULL;
}

#if __POSIX_VISIBLE >= 200112

int
setenv(const char *name, const char *value, int overwrite)
{
	if (!name || *name == '\0' || strchr(name, '=')) {
		errno = EINVAL;
		return -1;
	}

	if (!value) {
		value = "";
	}

	if (init_environ() < 0) {
		return -1;
	}

	size_t name_len = strlen(name);
	int idx = find_env(name, name_len);

	/* Variable exists */
	if (idx >= 0) {
		if (!overwrite) {
			return 0;
		}

		/* Replace existing */
		size_t total_len = name_len + 1 + strlen(value) + 1;
		char *new_str = (char *)malloc(total_len);
		
		if (!new_str) {
			errno = ENOMEM;
			return -1;
		}

		snprintf(new_str, total_len, "%s=%s", name, value);
		free(environ[idx]);
		environ[idx] = new_str;
		return 0;
	}

	/* Add new variable */
	if (environ_count >= environ_size - 1) {
		if (grow_environ() < 0) {
			return -1;
		}
	}

	size_t total_len = name_len + 1 + strlen(value) + 1;
	char *new_str = (char *)malloc(total_len);
	
	if (!new_str) {
		errno = ENOMEM;
		return -1;
	}

	snprintf(new_str, total_len, "%s=%s", name, value);
	environ[environ_count++] = new_str;
	environ[environ_count] = NULL;

	return 0;
}

int
unsetenv(const char *name)
{
	if (!name || *name == '\0' || strchr(name, '=')) {
		errno = EINVAL;
		return -1;
	}

	if (!environ) {
		return 0;
	}

	size_t len = strlen(name);
	int idx = find_env(name, len);

	if (idx < 0) {
		return 0;
	}

	/* Free the entry */
	free(environ[idx]);

	/* Shift remaining entries */
	for (size_t i = idx; i < environ_count; i++) {
		environ[i] = environ[i + 1];
	}

	environ_count--;
	return 0;
}

int
putenv(char *string)
{
	if (!string) {
		errno = EINVAL;
		return -1;
	}

	char *eq = strchr(string, '=');
	if (!eq) {
		errno = EINVAL;
		return -1;
	}

	if (init_environ() < 0) {
		return -1;
	}

	size_t name_len = eq - string;
	int idx = find_env(string, name_len);

	/* Variable exists - replace */
	if (idx >= 0) {
		free(environ[idx]);
		environ[idx] = string;
		return 0;
	}

	/* Add new variable */
	if (environ_count >= environ_size - 1) {
		if (grow_environ() < 0) {
			return -1;
		}
	}

	environ[environ_count++] = string;
	environ[environ_count] = NULL;

	return 0;
}

#endif /* __POSIX_VISIBLE >= 200112 */