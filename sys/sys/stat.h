#ifndef _SYS_STAT_H_
#define _SYS_STAT_H_

#include <sys/types.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

#define S_IFMT 0170000   /* Type of file mask */
#define S_IFIFO 0010000  /* Named pipe (fifo) */
#define S_IFCHR 0020000  /* Character special */
#define S_IFDIR 0040000  /* Directory */
#define S_IFBLK 0060000  /* Block special */
#define S_IFREG 0100000  /* Regular file */
#define S_IFLNK 0120000  /* Symbolic link */
#define S_IFSOCK 0140000 /* Socket */

#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)   /* Directory */
#define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)   /* Char special */
#define S_ISBLK(m) (((m) & S_IFMT) == S_IFBLK)   /* Block special */
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)   /* Regular file */
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)  /* Fifo */
#define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)   /* Symbolic link */
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK) /* Socket */

#define S_ISUID 0004000 /* Set user ID on execution */
#define S_ISGID 0002000 /* Set group ID on execution */
#define S_ISVTX 0001000 /* Sticky bit */

#define S_IRWXU 0000700 /* RWX mask for owner */
#define S_IRUSR 0000400 /* Read permission, owner */
#define S_IWUSR 0000200 /* Write permission, owner */
#define S_IXUSR 0000100 /* Execute/search permission, owner */

#define S_IRWXG 0000070 /* RWX mask for group */
#define S_IRGRP 0000040 /* Read permission, group */
#define S_IWGRP 0000020 /* Write permission, group */
#define S_IXGRP 0000010 /* Execute/search permission, group */

#define S_IRWXO 0000007 /* RWX mask for other */
#define S_IROTH 0000004 /* Read permission, other */
#define S_IWOTH 0000002 /* Write permission, other */
#define S_IXOTH 0000001 /* Execute/search permission, other */

struct stat {
	dev_t st_dev;         /* Device ID */
	ino_t st_ino;         /* Inode number */
	mode_t st_mode;       /* File type and mode */
	nlink_t st_nlink;     /* Number of hard links */
	uid_t st_uid;         /* User ID of owner */
	gid_t st_gid;         /* Group ID of owner */
	dev_t st_rdev;        /* Device ID (if special file) */
	off_t st_size;        /* Total size in bytes */
	blksize_t st_blksize; /* Block size for filesystem I/O */
	blkcnt_t st_blocks;   /* Number of 512B blocks allocated */
	time_t st_atime;      /* Time of last access */
	time_t st_mtime;      /* Time of last modification */
	time_t st_ctime;      /* Time of last status change */
};

#ifdef _KERNEL
/* Empty for now until full implementation */
#endif

__END_DECLS

#endif