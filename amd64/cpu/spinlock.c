#include <sys/spinlock.h>
#include <sys/atomic.h>
#include <sys/panic.h>
#include <sys/types.h>
#include <stddef.h>
#include <stdbool.h>
#include <intr.h>

#ifdef SPINLOCK_DEBUG
#include <printf.h>
#endif

static inline int
get_cpu_id(void)
{
	/* TODO: Implement proper CPU ID retrieval for SMP */
	/* This could read from GS segment base or APIC ID */
	return 0;
}

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

	lock->locked = 0;
	lock->cpu = -1;
	lock->name = name;
	lock->intr_save = 0;

#ifdef SPINLOCK_DEBUG
	lock->last_pc = NULL;
	lock->last_file = NULL;
	lock->last_line = 0;
	lock->acquire_time = 0;
	lock->hold_count = 0;
#endif
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

	while (atomic_swap_uint(&lock->locked, 1) != 0) {
		/* Spin with PAUSE to reduce memory bus traffic */
		/* Use atomic read to ensure we see latest value */
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
	/* TODO: Add timestamp when TSC is available */
#endif
}

void
spinlock_release(spinlock_t *lock)
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

	flags = READ_ONCE(lock->intr_save);

#ifdef SPINLOCK_DEBUG
	lock->last_pc = NULL;
#endif

	WRITE_ONCE(lock->cpu, -1);

	membar_exit_before_atomic();

	atomic_store_int(&lock->locked, 0);

	intr_restore(flags);
}

bool
spinlock_try_acquire(spinlock_t *lock)
{
	int cpu;
	uint64_t flags;

	if (lock == NULL) {
		panic("spinlock_try_acquire: NULL lock pointer");
	}

	flags = intr_disable();

	cpu = get_cpu_id();

	if (spinlock_holding(lock)) {
		intr_restore(flags);
		return false;
	}

	if (atomic_swap_uint(&lock->locked, 1) != 0) {
		intr_restore(flags);
		return false; /* Already locked */
	}

	membar_enter_after_atomic();

	WRITE_ONCE(lock->cpu, cpu);
	WRITE_ONCE(lock->intr_save, flags);

#ifdef SPINLOCK_DEBUG
	lock->last_pc = __builtin_return_address(0);
	lock->hold_count++;
#endif

	return true;
}

bool
spinlock_holding(spinlock_t *lock)
{
	int cpu;

	if (lock == NULL) {
		return false;
	}

	cpu = get_cpu_id();
	return READ_ONCE(lock->locked) != 0 && READ_ONCE(lock->cpu) == cpu;
}

#ifdef SPINLOCK_DEBUG
void
spinlock_acquire_irqsave_debug(spinlock_t *lock,
                               uint64_t *flags,
                               const char *file,
                               int line)
#else
void
spinlock_acquire_irqsave(spinlock_t *lock, uint64_t *flags)
#endif
{
	if (lock == NULL) {
		panic("spinlock_acquire_irqsave: NULL lock pointer");
	}

	if (flags == NULL) {
		panic("spinlock_acquire_irqsave: NULL flags pointer");
	}

	*flags = read_rflags();

	cli();

#ifdef SPINLOCK_DEBUG
	spinlock_acquire_debug(lock, file, line);
#else
	spinlock_acquire(lock);
#endif
}

void
spinlock_release_irqrestore(spinlock_t *lock, uint64_t flags)
{
	if (lock == NULL) {
		panic("spinlock_release_irqrestore: NULL lock pointer");
	}

	WRITE_ONCE(lock->intr_save, flags);

	spinlock_release(lock);
}

#ifdef SPINLOCK_DEBUG

void
spinlock_print_stats(const spinlock_t *lock)
{
	if (lock == NULL) {
		return;
	}

	printf("Spinlock '%s' statistics:\n", spinlock_get_name(lock));
	printf("  Locked: %s\n", lock->locked ? "yes" : "no");
	printf("  CPU: %d\n", lock->cpu);
	printf("  Hold count: %llu\n", (unsigned long long)lock->hold_count);

	if (lock->last_file) {
		printf("  Last acquired: %s:%d\n",
		       lock->last_file,
		       lock->last_line);
	}

	if (lock->last_pc) {
		printf("  Last PC: %p\n", lock->last_pc);
	}
}

#endif