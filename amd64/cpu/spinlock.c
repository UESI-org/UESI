#include <sys/spinlock.h>
#include <sys/atomic.h>
#include <sys/panic.h>
#include <sys/types.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef SPINLOCK_DEBUG
#include <printf.h>
#endif

static inline uint64_t
read_rflags(void)
{
	uint64_t flags;
	__asm__ volatile("pushfq; pop %0" : "=r"(flags)::"memory");
	return flags;
}

static inline void
write_rflags(uint64_t flags)
{
	__asm__ volatile("push %0; popfq" ::"r"(flags) : "memory", "cc");
}

static inline void
cli(void)
{
	__asm__ volatile("cli" ::: "memory");
}

static inline void
sti(void)
{
	__asm__ volatile("sti" ::: "memory");
}

static inline bool
interrupts_enabled(void)
{
	return (read_rflags() & 0x200) != 0; /* IF flag is bit 9 */
}

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
	if (lock == NULL) {
		panic("spinlock_acquire: NULL lock pointer");
	}

	if (spinlock_holding(lock)) {
		panic_fmt("spinlock_acquire: recursive lock detected on '%s' "
		          "by CPU %d",
		          spinlock_get_name(lock),
		          get_cpu_id());
	}

	while (atomic_swap_uint(&lock->locked, 1) != 0) {
		/* Spin with PAUSE to reduce memory bus traffic */
		while (lock->locked != 0) {
			cpu_relax();
		}
	}

	membar_enter_after_atomic();

	lock->cpu = get_cpu_id();

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
	if (lock == NULL) {
		panic("spinlock_release: NULL lock pointer");
	}

	if (!spinlock_holding(lock)) {
		panic_fmt("spinlock_release: lock '%s' not held by CPU %d",
		          spinlock_get_name(lock),
		          get_cpu_id());
	}

#ifdef SPINLOCK_DEBUG
	lock->last_pc = NULL;
#endif

	lock->cpu = -1;

	membar_exit_before_atomic();

	lock->locked = 0;
}

bool
spinlock_try_acquire(spinlock_t *lock)
{
	if (lock == NULL) {
		panic("spinlock_try_acquire: NULL lock pointer");
	}

	if (spinlock_holding(lock)) {
		return false;
	}

	if (atomic_swap_uint(&lock->locked, 1) != 0) {
		return false; /* Already locked */
	}

	membar_enter_after_atomic();

	lock->cpu = get_cpu_id();

#ifdef SPINLOCK_DEBUG
	lock->last_pc = __builtin_return_address(0);
	lock->hold_count++;
#endif

	return true;
}

bool
spinlock_holding(spinlock_t *lock)
{
	if (lock == NULL) {
		return false;
	}

	return lock->locked != 0 && lock->cpu == get_cpu_id();
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

	spinlock_release(lock);

	write_rflags(flags);
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