#include <stddef.h>
#include <dirent.h>
#include <unistd.h>

void
rewinddir(DIR *dirp)
{
	if (dirp == NULL) {
		return;
	}

	lseek(dirp->fd, 0, SEEK_SET);
	dirp->buf_pos = 0;
	dirp->buf_end = 0;
}