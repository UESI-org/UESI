/*
 * Copyright (c) 2024 UESI Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SYS_SYSTM_H_
#define _SYS_SYSTM_H_

#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/stdarg.h>
#include <sys/queue.h>

__BEGIN_DECLS

/*
 * Kernel identity and versioning
 */
extern const char ostype[];       /* "UESI" */
extern const char osrelease[];    /* "0.1" */
extern const char osversion[];    /* Full version string */
extern const char sccs[];         /* SCCS version string */
extern const char version[];      /* Complete build version */

/*
 * System state
 */
extern int cold;                  /* System still in early boot */
extern const char *panicstr;      /* Set when panic() is called */

/*
 * Hardware information (from bootloader/ACPI)
 */
extern char *hw_vendor;           /* Hardware vendor string */
extern char *hw_prod;             /* Hardware product string */
extern char *hw_uuid;             /* System UUID */
extern char *hw_serial;           /* System serial number */
extern char *hw_ver;              /* Hardware version */

/*
 * CPU information
 */
extern int ncpus;                 /* Number of CPUs in use */
extern int ncpusfound;            /* Number of CPUs detected */

struct cpu_info {
    volatile u_int ci_randseed;   /* Per-CPU random seed */
    /* TODO: Add more CPU-specific fields as needed */
};

/* TODO: Implement proper per-CPU data structure */
extern struct cpu_info cpu_info_primary;

static inline struct cpu_info *
curcpu(void)
{
    /* For now, single-CPU system - return primary CPU */
    return &cpu_info_primary;
}

/*
 * Temporarily undefine MIN/MAX/min/max macros from param.h
 * so libkern.h function declarations work correctly
 */
#ifdef MIN
#undef MIN
#endif
#ifdef MAX
#undef MAX
#endif
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

/*
 * Include libkern prototypes
 */
#include <libkern.h>

/*
 * Restore MIN/MAX/min/max macros from param.h
 */
#ifndef MIN
#define MIN(a,b)        (((a)<(b))?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b)        (((a)>(b))?(a):(b))
#endif
#ifndef min
#define min(a,b)        MIN(a,b)
#endif
#ifndef max
#define max(a,b)        MAX(a,b)
#endif

/*
 * Memory information
 */
extern uint64_t physmem;          /* Total physical memory (bytes) */

/*
 * Device information
 */
extern int nblkdev;               /* Number of block devices */
extern int nchrdev;               /* Number of character devices */

/*
 * Root and swap devices (TODO: implement)
 */
/* extern dev_t rootdev; */
/* extern dev_t swapdev; */
/* struct vnode; */
/* extern struct vnode *rootvp; */

/*
 * Process/thread management
 */
struct proc;
struct process;

/* Current process/thread accessor */
#define curproc         (proc_get_current())
#define curprocess      (curproc ? curproc->p_p : NULL)

/*
 * Panic and assertion functions
 */
__dead void panic(const char *fmt, ...)
    __attribute__((__noreturn__, __format__(__printf__, 1, 2)));

__dead void panic_fmt(const char *fmt, ...)
    __attribute__((__noreturn__, __format__(__printf__, 1, 2)));

#ifndef KASSERT
#ifndef NDEBUG
#define KASSERT(expr)                                                   \
    do {                                                                \
        if (!(expr))                                                    \
            panic("assertion \"%s\" failed: file \"%s\", line %d",     \
                #expr, __FILE__, __LINE__);                             \
    } while (0)
#else
#define KASSERT(expr)   ((void)0)
#endif
#endif

void    __assert(const char *, const char *, int, const char *)
    __attribute__((__noreturn__));

/*
 * Printf-like functions
 */
int printf(const char *fmt, ...)
    __attribute__((__format__(__printf__, 1, 2)));

int vprintf(const char *fmt, va_list ap)
    __attribute__((__format__(__printf__, 1, 0)));

int snprintf(char *buf, size_t size, const char *fmt, ...)
    __attribute__((__format__(__printf__, 3, 4)));

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
    __attribute__((__format__(__printf__, 3, 0)));

struct tty;
void ttyprintf(struct tty *tp, const char *fmt, ...)
    __attribute__((__format__(__printf__, 2, 3)));

/*
 * Kernel memory functions (from libkern)
 */
void    bcopy(const void *src, void *dst, size_t len)
    __attribute__((__bounded__(__buffer__, 1, 3)))
    __attribute__((__bounded__(__buffer__, 2, 3)));

void    bzero(void *buf, size_t len)
    __attribute__((__bounded__(__buffer__, 1, 2)));

void    explicit_bzero(void *buf, size_t len)
    __attribute__((__bounded__(__buffer__, 1, 2)));

int     bcmp(const void *b1, const void *b2, size_t len);

void   *memcpy(void *dst, const void *src, size_t len)
    __attribute__((__bounded__(__buffer__, 1, 3)))
    __attribute__((__bounded__(__buffer__, 2, 3)));

void   *memmove(void *dst, const void *src, size_t len)
    __attribute__((__bounded__(__buffer__, 1, 3)))
    __attribute__((__bounded__(__buffer__, 2, 3)));

void   *memset(void *buf, int c, size_t len)
    __attribute__((__bounded__(__buffer__, 1, 3)));

int     memcmp(const void *b1, const void *b2, size_t len);

/*
 * User/kernel space copy functions (TODO: implement)
 */
/* int     copyin(const void *uaddr, void *kaddr, size_t len); */
/* int     copyout(const void *kaddr, void *uaddr, size_t len); */
/* int     copyinstr(const void *uaddr, void *kaddr, size_t len, size_t *done); */
/* int     copyoutstr(const void *kaddr, void *uaddr, size_t len, size_t *done); */
/* int     kcopy(const void *src, void *dst, size_t len); */

/*
 * Scheduling and synchronization primitives (TODO: implement full set)
 */
struct spinlock;
/* struct mutex; */
/* struct rwlock; */

/* Sleep/wakeup (TODO: implement) */
/* void    wakeup(const volatile void *chan); */
/* void    wakeup_n(const volatile void *chan, int n); */
/* #define wakeup_one(c)   wakeup_n((c), 1) */

/* int     tsleep(const volatile void *chan, int priority, const char *wmesg, */
/*             int timo); */
/* int     tsleep_nsec(const volatile void *chan, int priority, */
/*             const char *wmesg, uint64_t nsecs); */

/* int     msleep(const volatile void *chan, struct mutex *mtx, int priority, */
/*             const char *wmesg, int timo); */
/* int     msleep_nsec(const volatile void *chan, struct mutex *mtx, */
/*             int priority, const char *wmesg, uint64_t nsecs); */

void    yield(void);  /* Already implemented via scheduler_yield() */

/* Infinite and maximum sleep times */
#define INFSLP          UINT64_MAX
#define MAXTSLP         (UINT64_MAX - 1)

/*
 * Time conversion utilities (TODO: implement)
 */
/* struct timeval; */
/* struct timespec; */

/* int     tvtohz(const struct timeval *tv); */
/* int     tstohz(const struct timespec *ts); */

/*
 * Clock and timer functions (TODO: implement full support)
 */
/* extern uint64_t hardclock_period; */
/* extern uint64_t statclock_avg; */
/* extern int statclock_is_randomized; */

/* struct clockframe; */
/* void    hardclock(struct clockframe *frame); */

/* void    initclocks(void); */
/* void    cpu_initclocks(void); */
/* void    cpu_startclock(void); */

/*
 * Random number generation (TODO: implement)
 */
/* void    random_start(int); */
/* void    enqueue_randomness(unsigned int); */
/* void    suspend_randomness(void); */
/* void    resume_randomness(char *buf, size_t len); */

/* uint32_t arc4random(void); */
/* uint32_t arc4random_uniform(uint32_t upper_bound); */
/* void    arc4random_buf(void *buf, size_t len); */

/*
 * Stub functions for unimplemented operations
 */
int     nullop(void *);
int     enodev(void);
int     enosys(void);
int     enoioctl(void);
int     enxio(void);
int     eopnotsupp(void *);

/*
 * Hash table allocation (TODO: implement)
 */
/* void   *hashinit(int count, int type, int flags, u_long *hashmask); */
/* void    hashfree(void *hash, int type, int flags); */

/*
 * Diagnostic message (TODO: implement)
 */
/* void    tablefull(const char *tab); */

/*
 * Startup hooks - functions to run after scheduler starts but
 * before any user processes (TODO: implement)
 */
/* struct hook_desc { */
/*     TAILQ_ENTRY(hook_desc) hd_list; */
/*     void    (*hd_fn)(void *); */
/*     void    *hd_arg; */
/* }; */
/* TAILQ_HEAD(hook_desc_head, hook_desc); */

/* extern struct hook_desc_head startuphook_list; */

/* void   *hook_establish(struct hook_desc_head *head, int order, */
/*             void (*fn)(void *), void *arg); */
/* void    hook_disestablish(struct hook_desc_head *head, void *cookie); */
/* void    dohooks(struct hook_desc_head *head, int flags); */

/* #define HOOK_REMOVE     0x01 */
/* #define HOOK_FREE       0x02 */

/* #define startuphook_establish(fn, arg) \ */
/*     hook_establish(&startuphook_list, 1, (fn), (arg)) */
/* #define startuphook_disestablish(vhook) \ */
/*     hook_disestablish(&startuphook_list, (vhook)) */
/* #define dostartuphooks() \ */
/*     dohooks(&startuphook_list, HOOK_REMOVE | HOOK_FREE) */

/*
 * I/O operations (TODO: implement)
 */
/* struct uio; */
/* int     uiomove(void *buf, size_t n, struct uio *uio); */

/*
 * System startup (TODO: implement)
 */
/* void    cpu_startup(void); */
/* void    cpu_configure(void); */
/* void    consinit(void); */

/*
 * Delay function (microseconds) (TODO: implement)
 */
/* void    delay(unsigned int usecs); */
/* #define DELAY(n)        delay(n) */

/*
 * Boot configuration
 */
#ifdef BOOT_CONFIG
void    user_config(void);
#endif

/*
 * Kernel debugger (kdebug)
 */
#ifdef KDEBUG
void    kdebug_enter(void);
int     kdebug_active(void);
#endif

/*
 * Temporarily undefine MIN/MAX/min/max macros from param.h
 * so libkern.h function declarations work correctly
 */
#ifdef MIN
#undef MIN
#endif
#ifdef MAX
#undef MAX
#endif
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

/*
 * Include libkern prototypes
 */
#include <libkern.h>

/*
 * Restore MIN/MAX/min/max macros from param.h
 */
#ifndef MIN
#define MIN(a,b)        (((a)<(b))?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b)        (((a)>(b))?(a):(b))
#endif
#ifndef min
#define min(a,b)        MIN(a,b)
#endif
#ifndef max
#define max(a,b)        MAX(a,b)
#endif

__END_DECLS

#endif /* !_SYS_SYSTM_H_ */