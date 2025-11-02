#ifndef _SYS_TYPES_H_
#define	_SYS_TYPES_H_

#include <stdint.h>
#include <stddef.h>

typedef	uint32_t	dev_t;

					/* major part of a device */
#define	major(x)	((int)(((unsigned)(x)>>8)&0377))
					/* minor part of a device */
#define	minor(x)	((int)((x)&0377))
					/* make a device number */
#define	makedev(x,y)	((dev_t)(((x)<<8) | (y)))

typedef	uint8_t		u_char;
typedef	uint16_t	u_short;
typedef	uint32_t	u_int;
typedef	uint64_t	u_long;
typedef	uint16_t	ushort;		/* Sys V compatibility */

typedef	int64_t		clock_t;
typedef	int64_t		time_t;
typedef	uint64_t	size_t;
typedef	int64_t		ssize_t;

typedef	uint64_t	u_quad;
typedef	int64_t		quad;

typedef	int64_t *	qaddr_t;
typedef	int64_t		daddr_t;
typedef	char *		caddr_t;
typedef	uint64_t	ino_t;
typedef	int64_t		swblk_t;
typedef	int64_t		segsz_t;
typedef	int64_t		off_t;

typedef	uint32_t	uid_t;
typedef	uint32_t	gid_t;
typedef	int32_t		pid_t;

typedef	uint32_t	nlink_t;
typedef	uint32_t	mode_t;
typedef uint64_t	fixpt_t;

#define	NBBY	8		/* number of bits in a byte */

/*
 * Select uses bit masks of file descriptors in longs.  These macros
 * manipulate such bit fields (the filesystem macros use chars).
 * FD_SETSIZE may be defined by the user, but the default here should
 * be >= NOFILE (param.h).
 */
#ifndef	FD_SETSIZE
#define	FD_SETSIZE	1024
#endif

typedef uint64_t	fd_mask;
#define NFDBITS		(sizeof(fd_mask) * NBBY)	/* bits per mask */

#ifndef howmany
#define	howmany(x, y)	(((x)+((y)-1))/(y))
#endif

typedef	struct fd_set {
	fd_mask	fds_bits[howmany(FD_SETSIZE, NFDBITS)];
} fd_set;

#define	FD_SET(n, p)	((p)->fds_bits[(n)/NFDBITS] |= (1UL << ((n) % NFDBITS)))
#define	FD_CLR(n, p)	((p)->fds_bits[(n)/NFDBITS] &= ~(1UL << ((n) % NFDBITS)))
#define	FD_ISSET(n, p)	((p)->fds_bits[(n)/NFDBITS] & (1UL << ((n) % NFDBITS)))

#ifdef KERNEL
#define	FD_ZERO(p)	__builtin_memset((p), 0, sizeof(*(p)))
#else
#define	FD_ZERO(p)	memset((p), 0, sizeof(*(p)))
#endif

#ifndef NULL
#define NULL		((void *)0)
#endif

typedef int		bool_t;
#ifndef TRUE
#define TRUE		1
#define FALSE		0
#endif

#endif /* !_SYS_TYPES_H_ */