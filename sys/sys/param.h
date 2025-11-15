#ifndef _SYS_PARAM_H_
#define	_SYS_PARAM_H_

#define	MAXUPRC		128		/* max processes per user */
#define	NOFILE		256		/* max open files per process */
#define	NCARGS		65536		/* # characters in exec arglist (64KB) */
#define	MAXINTERP	128		/* max interpreter file name length */
#define	NGROUPS		32		/* max number groups */
#define MAXHOSTNAMELEN	256		/* maximum hostname size */
#define	MAXCOMLEN	16		/* maximum command name remembered */
	/* MAXCOMLEN should be >= sizeof(ac_comm) (acct.h)  */
#define	MAXLOGNAME	32		/* maximum login name length */

#define	NOGROUP		65535		/* marker for empty group set member */

#define	PSWP	0
#define	PINOD	10
#define	PRIBIO	20
#define	PVFS	22
#define	PZERO	25
#define	PSOCK	26
#define	PWAIT	30
#define	PLOCK	35
#define	PPAUSE	40
#define	PUSER	50
#define	PRIMASK	0x0ff
#define	PCATCH	0x100		/* or'd with pri for tsleep to check signals */

#define	NZERO	0

#ifndef KERNEL
#include <types.h>
#endif

#define	NBPG		4096		/* bytes/page */
#define	PGOFSET		(NBPG-1)	/* byte offset into page */
#define	PGSHIFT		12		/* LOG2(NBPG) */

#define	NBPW		8		/* number of bytes in a word (64-bit) */

#ifndef NULL
#define	NULL		((void *)0)
#endif
#define	CMASK		022		/* default mask for file creation */
#define	NODEV		(dev_t)(-1)

/*
 * Clustering of hardware pages on machines with ridiculously small
 * page sizes is done here.  The paging subsystem deals with units of
 * CLSIZE pte's describing NBPG pages each.
 *
 * NOTE: On x86_64, we use CLSIZE=1 since 4KB pages are reasonable
 */
#define	CLSIZE		1
#define	CLSIZELOG2	0

#define	CLBYTES		(CLSIZE*NBPG)
#define	CLOFSET		(CLSIZE*NBPG-1)	/* for clusters, like PGOFSET */
#define	claligned(x)	((((long)(x))&CLOFSET)==0)
#define	CLOFF		CLOFSET
#define	CLSHIFT		(PGSHIFT+CLSIZELOG2)

#define	clbase(i)	(i)
#define	clrnd(i)	(i)

#define	CBLOCK		64
#define CBQSIZE		(CBLOCK/NBBY)	/* quote bytes/cblock - can do better */
#define	CBSIZE		(CBLOCK - sizeof(struct cblock *) - CBQSIZE) /* data chars/clist */
#define	CROUND		(CBLOCK - 1)	/* clist rounding */

/*
 * File system parameters and macros.
 *
 * The file system is made out of blocks of at most MAXBSIZE units,
 * with smaller units (fragments) only in the last direct block.
 * MAXBSIZE primarily determines the size of buffers in the buffer
 * pool. It may be made larger without any effect on existing
 * file systems; however making it smaller make make some file
 * systems unmountable.
 */
#define	MAXBSIZE	65536		/* increased for modern systems */
#define MAXFRAG 	8

/*
 * MAXPATHLEN defines the longest permissable path length
 * after expanding symbolic links. It is used to allocate
 * a temporary buffer from the buffer pool in which to do the
 * name expansion, hence should be a power of two, and must
 * be less than or equal to MAXBSIZE.
 * MAXSYMLINKS defines the maximum number of symbolic links
 * that may be expanded in a path name. It should be set high
 * enough to allow all legitimate uses, but halt infinite loops
 * reasonably quickly.
 */
#define	MAXPATHLEN	4096		/* maximum path length */
#define MAXSYMLINKS	40		/* increased for modern filesystems */

#define	setbit(a,i)	((a)[(i)/NBBY] |= 1<<((i)%NBBY))
#define	clrbit(a,i)	((a)[(i)/NBBY] &= ~(1<<((i)%NBBY)))
#define	isset(a,i)	((a)[(i)/NBBY] & (1<<((i)%NBBY)))
#define	isclr(a,i)	(((a)[(i)/NBBY] & (1<<((i)%NBBY))) == 0)

#ifndef howmany
#define	howmany(x, y)	(((x)+((y)-1))/(y))
#endif
#define	roundup(x, y)	((((x)+((y)-1))/(y))*(y))
#define powerof2(x)	((((x)-1)&(x))==0)

#define	MIN(a,b)	(((a)<(b))?(a):(b))
#define	MAX(a,b)	(((a)>(b))?(a):(b))
#define	min(a,b)	MIN(a,b)
#define	max(a,b)	MAX(a,b)

/*
 * Constants for setting the parameters of the kernel memory allocator.
 *
 * 2 ** MINBUCKET is the smallest unit of memory that will be
 * allocated. It must be at least large enough to hold a pointer.
 *
 * Units of memory less or equal to MAXALLOCSAVE will permanently
 * allocate physical memory; requests for these size pieces of
 * memory are quite fast. Allocations greater than MAXALLOCSAVE must
 * always allocate and free physical memory; requests for these
 * size allocations should be done infrequently as they will be slow.
 * Constraints: CLBYTES <= MAXALLOCSAVE <= 2 ** (MINBUCKET + 14)
 * and MAXALLOCSIZE must be a power of two.
 */
#define MINBUCKET	4		/* 4 => min allocation of 16 bytes */
#define MAXALLOCSAVE	(2 * CLBYTES)	/* 8192 bytes on x86_64 */

/*
 * Scale factor for scaled integers used to count %cpu time and load avgs.
 *
 * The number of CPU `tick's that map to a unique `%age' can be expressed
 * by the formula (1 / (2 ^ (FSHIFT - 11))).  The maximum load average that
 * can be calculated (assuming 32 bits) can be closely approximated using
 * the formula (2 ^ (2 * (16 - FSHIFT))) for (FSHIFT < 15).
 *
 * For the scheduler to maintain a 1:1 mapping of CPU `tick' to `%age',
 * FSHIFT must be at least 11; this gives us a maximum load avg of ~1024.
 */
#define	FSHIFT		11		/* bits to right of fixed binary point */
#define FSCALE		(1<<FSHIFT)

#define KERNBASE	0xffffffff80000000UL	/* start of kernel space */
#define KERNSIZE	0x0000000040000000UL	/* 1GB kernel size */

#define	btop(x)		((unsigned long)(x) >> PGSHIFT)		/* bytes to pages */
#define	ptob(x)		((unsigned long)(x) << PGSHIFT)		/* pages to bytes */

#define	btodb(bytes)	((unsigned long)(bytes) >> 9)		/* bytes to disk blocks */
#define	dbtob(db)	((unsigned long)(db) << 9)		/* disk blocks to bytes */

/*
 * Map a ``block device block'' to a file system block.
 * This should be device dependent, and should use the bsize
 * field from the disk label.
 * For now though just use DEV_BSIZE.
 */
#define	DEV_BSIZE	512
#define	DEV_BSHIFT	9		/* log2(DEV_BSIZE) */
#define BLKDEV_IOSIZE	2048
#define	MAXPHYS		(64 * 1024)	/* max raw I/O transfer size */

#define	DELAY(n)	delay(n)

#endif /* !_SYS_PARAM_H_ */