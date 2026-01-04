#ifndef _SYS_SPINLOCK_H_
#define _SYS_SPINLOCK_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <stdbool.h>

__BEGIN_DECLS

typedef struct spinlock {
	volatile unsigned int locked;
	const char *name;
	int cpu;            /* CPU holding the lock */
	uint64_t intr_save; /* Saved interrupt state */
#ifdef SPINLOCK_DEBUG
	void *last_pc; /* Last PC that acquired the lock */
	const char *last_file;
	int last_line;
	uint64_t hold_count;    /* Number of times acquired */
	uint64_t acquire_tsc;   /* TSC when lock was acquired */
	uint64_t total_hold_ns; /* Total time held (nanoseconds) */
	uint64_t max_hold_ns;   /* Maximum hold time */
#endif
} spinlock_t;

#define SPINLOCK_HOLD_WARN_NS 1000000   /* Warn if held > 1ms */
#define SPINLOCK_HOLD_PANIC_NS 10000000 /* Panic if held > 10ms */

#ifdef SPINLOCK_DEBUG
#define SPINLOCK_INITIALIZER(lockname)                                         \
	{ .locked = 0,                                                         \
	  .cpu = -1,                                                           \
	  .name = (lockname),                                                  \
	  .intr_save = 0,                                                      \
	  .last_pc = NULL,                                                     \
	  .last_file = NULL,                                                   \
	  .last_line = 0,                                                      \
	  .hold_count = 0,                                                     \
	  .acquire_tsc = 0,                                                    \
	  .total_hold_ns = 0,                                                  \
	  .max_hold_ns = 0 }
#else
#define SPINLOCK_INITIALIZER(lockname)                                         \
	{ .locked = 0, .cpu = -1, .name = (lockname), .intr_save = 0 }
#endif

void spinlock_init(spinlock_t *lock, const char *name);

#ifdef SPINLOCK_DEBUG
void spinlock_acquire_debug(spinlock_t *lock, const char *file, int line);
void spinlock_release_debug(spinlock_t *lock, const char *file, int line);
#define spinlock_acquire(lock)                                                 \
	spinlock_acquire_debug((lock), __FILE__, __LINE__)
#define spinlock_release(lock)                                                 \
	spinlock_release_debug((lock), __FILE__, __LINE__)
#else
void spinlock_acquire(spinlock_t *lock);
void spinlock_release(spinlock_t *lock);
#endif

bool spinlock_try_acquire(spinlock_t *lock);
bool spinlock_holding(spinlock_t *lock);
void spinlock_acquire_irqsave(spinlock_t *lock, uint64_t *flags);
void spinlock_release_irqrestore(spinlock_t *lock, uint64_t flags);

static inline int
spinlock_get_cpu(const spinlock_t *lock)
{
	return lock->cpu;
}

static inline const char *
spinlock_get_name(const spinlock_t *lock)
{
	return lock->name ? lock->name : "(unnamed)";
}

#ifdef SPINLOCK_DEBUG
void spinlock_print_stats(const spinlock_t *lock);
#endif

__END_DECLS

#endif
