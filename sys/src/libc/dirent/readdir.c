#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>

struct dirent *
readdir(DIR *dirp)
{
	if (dirp == NULL) {
		errno = EBADF;
		return NULL;
	}

	if (dirp->buf_pos >= dirp->buf_end) {
		int nread = getdents(dirp->fd, dirp->buf, sizeof(dirp->buf));
		if (nread <= 0) {
			if (nread < 0) {
				/* Error already set by getdents */
				return NULL;
			}
			/* End of directory */
			return NULL;
		}
		dirp->buf_pos = 0;
		dirp->buf_end = nread;
	}

	struct linux_dirent {
		unsigned long d_ino;
		unsigned long d_off;
		unsigned short d_reclen;
		char d_name[];
	};

	struct linux_dirent *lde = (struct linux_dirent *)(dirp->buf + dirp->buf_pos);
	dirp->buf_pos += lde->d_reclen;

	static struct dirent result;
	result.d_ino = lde->d_ino;
	result.d_off = lde->d_off;
	result.d_reclen = sizeof(struct dirent);
	
	char *type_ptr = (char *)lde + lde->d_reclen - 1;
	result.d_type = *type_ptr;
	
	size_t name_len = strlen(lde->d_name);
	if (name_len >= sizeof(result.d_name)) {
		name_len = sizeof(result.d_name) - 1;
	}
	memcpy(result.d_name, lde->d_name, name_len);
	result.d_name[name_len] = '\0';

	return &result;
}