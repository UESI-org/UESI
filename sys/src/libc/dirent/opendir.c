#include <fcntl.h>
#include <stdlib.h>
#include <stddef.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>

DIR *
opendir(const char *name)
{
	int fd = open(name, O_RDONLY | O_DIRECTORY, 0);
	if (fd < 0) {
		return NULL;
	}

	DIR *dirp = malloc(sizeof(DIR));
	if (dirp == NULL) {
		close(fd);
		errno = ENOMEM;
		return NULL;
	}

	dirp->fd = fd;
	dirp->buf_pos = 0;
	dirp->buf_end = 0;

	return dirp;
}