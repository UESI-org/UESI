#ifndef _SYS_SPINLOCK_H_
#define _SYS_SPINLOCK_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <stdbool.h>

__BEGIN_DECLS

typedef struct spinlock {
	volatile unsigned int locked;
	int cpu;
	const char *name;
#ifdef SPINLOCK_DEBUG
	void *last_pc;
	const char *last_file;
	int last_line;
	uint64_t acquire_time;
	uint64_t hold_count;
#endif
} spinlock_t;

#ifdef SPINLOCK_DEBUG
#define SPINLOCK_INITIALIZER(lockname) \
	{ .locked = 0, .cpu = -1, .name = (lockname), \
	  .last_pc = NULL, .last_file = NULL, .last_line = 0, \
	  .acquire_time = 0, .hold_count = 0 }
#else
#define SPINLOCK_INITIALIZER(lockname) \
	{ .locked = 0, .cpu = -1, .name = (lockname) }
#endif

void spinlock_init(spinlock_t *lock, const char *name);

void spinlock_acquire(spinlock_t *lock);

void spinlock_release(spinlock_t *lock);

bool spinlock_try_acquire(spinlock_t *lock);

bool spinlock_holding(spinlock_t *lock);

void spinlock_acquire_irqsave(spinlock_t *lock, uint64_t *flags);

void spinlock_release_irqrestore(spinlock_t *lock, uint64_t flags);

static inline int spinlock_get_cpu(const spinlock_t *lock) {
	return lock->cpu;
}

static inline const char *spinlock_get_name(const spinlock_t *lock) {
	return lock->name ? lock->name : "(unnamed)";
}

#ifdef SPINLOCK_DEBUG

void spinlock_acquire_debug(spinlock_t *lock, const char *file, int line);
void spinlock_acquire_irqsave_debug(spinlock_t *lock, uint64_t *flags, 
                                    const char *file, int line);

#define spinlock_acquire(lock) \
	spinlock_acquire_debug((lock), __FILE__, __LINE__)
#define spinlock_acquire_irqsave(lock, flags) \
	spinlock_acquire_irqsave_debug((lock), (flags), __FILE__, __LINE__)

void spinlock_print_stats(const spinlock_t *lock);
#endif

__END_DECLS

#endif