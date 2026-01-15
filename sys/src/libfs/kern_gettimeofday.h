#ifndef _KERN_GETTIMEOFDAY_H_
#define _KERN_GETTIMEOFDAY_H_

#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Kernel-side gettimeofday().
 *
 * This is currently used by in-kernel libc consumers (time(), VFS, etc.).
 * Later, userland gettimeofday() can be implemented as a syscall wrapper
 * that forwards here.
 */
int gettimeofday(struct timeval *tv, struct timezone *tz);

#ifdef __cplusplus
}
#endif

#endif /* _KERN_GETTIMEOFDAY_H_ */
