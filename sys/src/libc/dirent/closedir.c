#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <stddef.h>
#include <unistd.h>

int
closedir(DIR *dirp)
{
	if (dirp == NULL) {
		errno = EBADF;
		return -1;
	}

	int ret = close(dirp->fd);
	free(dirp);
	return ret;
}