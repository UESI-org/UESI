#include <sys/spinlock.h>
#include <sys/atomic.h>
#include <sys/panic.h>
#include <intr.h>
#include <printf.h>
#include <string.h>
#include <tsc.h>

static inline int
get_cpu_id(void)
{
	/* TODO: Implement proper CPU ID detection via APIC */
	/* For now, always return 0 (single CPU) */
	return 0;
}

/* CPU relaxation hint for spinloop */
static inline void
cpu_relax(void)
{
	__asm__ volatile("pause" ::: "memory");
}

void
spinlock_init(spinlock_t *lock, const char *name)
{
	if (lock == NULL) {
		panic("spinlock_init: NULL lock pointer");
	}

	memset(lock, 0, sizeof(spinlock_t));
	lock->name = name;
	lock->locked = 0;
	lock->cpu = -1;
}

bool
spinlock_holding(spinlock_t *lock)
{
	bool r;
	uint64_t flags;

	if (lock == NULL) {
		return false;
	}

	flags = intr_disable();
	r = (lock->locked && lock->cpu == get_cpu_id());
	intr_restore(flags);
	return r;
}

#ifdef SPINLOCK_DEBUG
void
spinlock_acquire_debug(spinlock_t *lock, const char *file, int line)
#else
void
spinlock_acquire(spinlock_t *lock)
#endif
{
	int cpu;
	uint64_t flags;
#ifdef SPINLOCK_DEBUG
	uint64_t start_spin_tsc = 0;
#endif

	if (lock == NULL) {
		panic("spinlock_acquire: NULL lock pointer");
	}

	flags = intr_disable();
	cpu = get_cpu_id();

	if (spinlock_holding(lock)) {
		intr_restore(flags);
		panic_fmt("spinlock_acquire: recursive lock detected on '%s' "
		          "by CPU %d",
		          spinlock_get_name(lock),
		          cpu);
	}

#ifdef SPINLOCK_DEBUG
	/* Record when we started spinning */
	if (tsc_is_available()) {
		start_spin_tsc = tsc_read();
	}
#endif

	while (atomic_swap_uint(&lock->locked, 1) != 0) {
		/* Spin with PAUSE to reduce memory bus traffic */
		while (atomic_load_int(&lock->locked) != 0) {
			cpu_relax();
		}
	}

	membar_enter_after_atomic();

	WRITE_ONCE(lock->cpu, cpu);
	WRITE_ONCE(lock->intr_save, flags);

#ifdef SPINLOCK_DEBUG
	lock->last_pc = __builtin_return_address(0);
	lock->last_file = file;
	lock->last_line = line;
	lock->hold_count++;

	/* Record acquisition time */
	if (tsc_is_available()) {
		lock->acquire_tsc = tsc_read();

		/* Warn if we spun for too long */
		uint64_t spin_time_ns =
		    tsc_to_ns(lock->acquire_tsc - start_spin_tsc);
		if (spin_time_ns > SPINLOCK_HOLD_WARN_NS) {
			printf(
			    "WARNING: spinlock '%s' spun for %lu ns at %s:%d\n",
			    spinlock_get_name(lock),
			    (unsigned long)spin_time_ns,
			    file,
			    line);
		}
	}
#endif
}

#ifdef SPINLOCK_DEBUG
void
spinlock_release_debug(spinlock_t *lock, const char *file, int line)
#else
void
spinlock_release(spinlock_t *lock)
#endif
{
	uint64_t flags;

	if (lock == NULL) {
		panic("spinlock_release: NULL lock pointer");
	}

	if (!spinlock_holding(lock)) {
		panic_fmt("spinlock_release: lock '%s' not held by CPU %d",
		          spinlock_get_name(lock),
		          get_cpu_id());
	}

#ifdef SPINLOCK_DEBUG
	/* Calculate and record hold time */
	if (tsc_is_available() && lock->acquire_tsc != 0) {
		uint64_t release_tsc = tsc_read();
		uint64_t hold_ns = tsc_to_ns(release_tsc - lock->acquire_tsc);

		lock->total_hold_ns += hold_ns;

		if (hold_ns > lock->max_hold_ns) {
			lock->max_hold_ns = hold_ns;
		}

		/* Warn if held too long */
		if (hold_ns > SPINLOCK_HOLD_WARN_NS) {
			printf("WARNING: spinlock '%s' held for %lu ns "
			       "(acquired at %s:%d, released at %s:%d)\n",
			       spinlock_get_name(lock),
			       (unsigned long)hold_ns,
			       lock->last_file,
			       lock->last_line,
			       file,
			       line);
		}

		/* Panic if held REALLY too long */
		if (hold_ns > SPINLOCK_HOLD_PANIC_NS) {
			panic_fmt(
			    "spinlock '%s' held for %lu ns (limit %lu ns)\n"
			    "  Acquired: %s:%d\n"
			    "  Released: %s:%d",
			    spinlock_get_name(lock),
			    (unsigned long)hold_ns,
			    (unsigned long)SPINLOCK_HOLD_PANIC_NS,
			    lock->last_file,
			    lock->last_line,
			    file,
			    line);
		}
	}
	lock->acquire_tsc = 0;
#endif

	WRITE_ONCE(lock->cpu, -1);

	flags = READ_ONCE(lock->intr_save);

	membar_exit_before_atomic();
	atomic_store_int(&lock->locked, 0);

	intr_restore(flags);
}

bool
spinlock_try_acquire(spinlock_t *lock)
{
	uint64_t flags;
	int cpu;
	bool acquired = false;

	if (lock == NULL) {
		return false;
	}

	flags = intr_disable();
	cpu = get_cpu_id();

	if (atomic_swap_uint(&lock->locked, 1) == 0) {
		membar_enter_after_atomic();
		WRITE_ONCE(lock->cpu, cpu);
		WRITE_ONCE(lock->intr_save, flags);
		acquired = true;
	} else {
		intr_restore(flags);
	}

	return acquired;
}

void
spinlock_acquire_irqsave(spinlock_t *lock, uint64_t *flags)
{
	if (flags != NULL) {
		*flags = intr_disable();
	}
	spinlock_acquire(lock);
}

void
spinlock_release_irqrestore(spinlock_t *lock, uint64_t flags)
{
	spinlock_release(lock);
	intr_restore(flags);
}

#ifdef SPINLOCK_DEBUG
/* Print spinlock statistics */
void
spinlock_print_stats(const spinlock_t *lock)
{
	if (lock == NULL) {
		printf("spinlock_print_stats: NULL lock\n");
		return;
	}

	printf("Spinlock '%s' statistics:\n", spinlock_get_name(lock));
	printf("  Hold count: %lu\n", (unsigned long)lock->hold_count);

	if (tsc_is_available() && lock->hold_count > 0) {
		uint64_t avg_hold_ns = lock->total_hold_ns / lock->hold_count;
		printf("  Total hold time: %lu ns\n",
		       (unsigned long)lock->total_hold_ns);
		printf("  Average hold time: %lu ns\n",
		       (unsigned long)avg_hold_ns);
		printf("  Maximum hold time: %lu ns\n",
		       (unsigned long)lock->max_hold_ns);
	}

	if (lock->last_file) {
		printf("  Last acquired: %s:%d\n",
		       lock->last_file,
		       lock->last_line);
	}

	printf("  Currently held: %s", lock->locked ? "yes" : "no");
	if (lock->locked) {
		printf(" (CPU %d)", lock->cpu);
	}
	printf("\n");
}
#endif
