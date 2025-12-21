#ifndef	_SYS_UTSNAME_H
#define	_SYS_UTSNAME_H

#define	SYS_NMLN	256

struct utsname {
	char	sysname[SYS_NMLN];	/* Name of this OS. */
	char	nodename[SYS_NMLN];	/* Name of this network node. */
	char	release[SYS_NMLN];	/* Release level. */
	char	version[SYS_NMLN];	/* Version level. */
	char	machine[SYS_NMLN];	/* Hardware type. */
};

#include <sys/cdefs.h>

#ifndef _KERNEL
__BEGIN_DECLS
int	uname(struct utsname *);
__END_DECLS
#endif /* !_KERNEL */

#ifdef _KERNEL
int	kern_uname(struct utsname *);
#endif

#endif	/* !_SYS_UTSNAME_H */