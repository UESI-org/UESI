#include <sys/mutex.h>
#include <sys/spinlock.h>
#include <sys/atomic.h>
#include <sys/panic.h>
#include <sys/types.h>
#include <sys/kernel.h>

#include <kmalloc.h>
#include <kfree.h>
#include <scheduler.h>
#include <printf.h>
#include <string.h>
#include <stdbool.h>

static inline void
mtx_waitq_enqueue(mutex_t *mtx, struct task *task)
{
	mtx_waitq_t *entry;

	entry = kmalloc(sizeof(mtx_waitq_t));
	if (entry == NULL) {
		panic("mtx_waitq_enqueue: kmalloc failed");
	}

	entry->task = task;
	entry->next = NULL;

	if (mtx->mtx_waitq_tail == NULL) {
		mtx->mtx_waitq_head = entry;
		mtx->mtx_waitq_tail = entry;
	} else {
		mtx->mtx_waitq_tail->next = entry;
		mtx->mtx_waitq_tail = entry;
	}
}

static inline struct task *
mtx_waitq_dequeue(mutex_t *mtx)
{
	mtx_waitq_t *entry;
	struct task *task;

	if (mtx->mtx_waitq_head == NULL) {
		return NULL;
	}

	entry = mtx->mtx_waitq_head;
	task = entry->task;

	mtx->mtx_waitq_head = entry->next;
	if (mtx->mtx_waitq_head == NULL) {
		mtx->mtx_waitq_tail = NULL;
	}

	kfree(entry);
	return task;
}

static inline bool
mtx_waitq_empty(const mutex_t *mtx)
{
	return mtx->mtx_waitq_head == NULL;
}

void
mutex_init(mutex_t *mtx, const char *name, unsigned int type)
{
	char lock_name[64];

	if (mtx == NULL) {
		panic("mutex_init: NULL mutex pointer");
	}

	/* Initialize the internal spinlock */
	if (name != NULL) {
		snprintf(lock_name, sizeof(lock_name), "mtx:%s", name);
		spinlock_init(&mtx->mtx_lock, lock_name);
	} else {
		spinlock_init(&mtx->mtx_lock, "mtx:(unnamed)");
	}

	mtx->mtx_owner = NULL;
	mtx->mtx_recurse = 0;
	mtx->mtx_type = type;
	mtx->mtx_name = name;
	mtx->mtx_waitq_head = NULL;
	mtx->mtx_waitq_tail = NULL;

#ifdef MTX_DEBUG
	mtx->mtx_file = NULL;
	mtx->mtx_line = 0;
	mtx->mtx_acquire_time = 0;
	mtx->mtx_contention_count = 0;
#endif
}

void
mutex_destroy(mutex_t *mtx)
{
	if (mtx == NULL) {
		panic("mutex_destroy: NULL mutex pointer");
	}

	spinlock_acquire(&mtx->mtx_lock);

	if (mtx->mtx_owner != NULL) {
		spinlock_release(&mtx->mtx_lock);
		panic_fmt("mutex_destroy: mutex '%s' is still locked",
		          mutex_name(mtx));
	}

	if (!mtx_waitq_empty(mtx)) {
		spinlock_release(&mtx->mtx_lock);
		panic_fmt("mutex_destroy: mutex '%s' has waiting tasks",
		          mutex_name(mtx));
	}

	spinlock_release(&mtx->mtx_lock);

	/* Clear the mutex structure */
	memset(mtx, 0, sizeof(*mtx));
}

void
mutex_lock(mutex_t *mtx)
{
	struct task *current_task;
	bool did_sleep;

	if (mtx == NULL) {
		panic("mutex_lock: NULL mutex pointer");
	}

	current_task = scheduler_get_current_task();
	if (current_task == NULL) {
		/* Early boot or interrupt context - just spin */
		if ((mtx->mtx_type & MTX_SPIN) == 0) {
			panic_fmt("mutex_lock: no current task for mutex '%s'",
			          mutex_name(mtx));
		}
		spinlock_acquire(&mtx->mtx_lock);
		return;
	}

	spinlock_acquire(&mtx->mtx_lock);

	/* Check for recursive locking */
	if (mtx->mtx_owner == current_task) {
		if (mtx->mtx_type & MTX_RECURSE) {
			mtx->mtx_recurse++;
			spinlock_release(&mtx->mtx_lock);
			return;
		} else {
			spinlock_release(&mtx->mtx_lock);
			panic_fmt("mutex_lock: recursive lock on non-recursive "
			          "mutex '%s'", mutex_name(mtx));
		}
	}

	/* Try to acquire the mutex */
	while (mtx->mtx_owner != NULL) {
		/* Mutex is held, add ourselves to wait queue */
		did_sleep = false;

#ifdef MTX_DEBUG
		mtx->mtx_contention_count++;
#endif

		if ((mtx->mtx_type & MTX_SPIN) == 0) {
			/* Add to wait queue and block */
			mtx_waitq_enqueue(mtx, current_task);
			scheduler_block_task(current_task);
			did_sleep = true;
		}

		spinlock_release(&mtx->mtx_lock);

		if (did_sleep) {
			/* Yield to scheduler - will wake us up later */
			scheduler_yield();
		} else {
			/* Spin with pause */
			__asm__ volatile("pause" ::: "memory");
		}

		spinlock_acquire(&mtx->mtx_lock);
	}

	/* Acquire the mutex */
	WRITE_ONCE(mtx->mtx_owner, current_task);
	mtx->mtx_recurse = 1;

#ifdef MTX_DEBUG
	mtx->mtx_acquire_time = get_uptime_ms();
#endif

	spinlock_release(&mtx->mtx_lock);
}

bool
mutex_trylock(mutex_t *mtx)
{
	struct task *current_task;

	if (mtx == NULL) {
		panic("mutex_trylock: NULL mutex pointer");
	}

	current_task = scheduler_get_current_task();
	if (current_task == NULL) {
		/* Early boot or interrupt context */
		if ((mtx->mtx_type & MTX_SPIN) == 0) {
			panic_fmt("mutex_trylock: no current task for mutex '%s'",
			          mutex_name(mtx));
		}
		return spinlock_try_acquire(&mtx->mtx_lock);
	}

	spinlock_acquire(&mtx->mtx_lock);

	/* Check for recursive locking */
	if (mtx->mtx_owner == current_task) {
		if (mtx->mtx_type & MTX_RECURSE) {
			mtx->mtx_recurse++;
			spinlock_release(&mtx->mtx_lock);
			return true;
		} else {
			spinlock_release(&mtx->mtx_lock);
			return false;
		}
	}

	/* Check if mutex is available */
	if (mtx->mtx_owner != NULL) {
		spinlock_release(&mtx->mtx_lock);
		return false;
	}

	/* Acquire the mutex */
	WRITE_ONCE(mtx->mtx_owner, current_task);
	mtx->mtx_recurse = 1;

#ifdef MTX_DEBUG
	mtx->mtx_acquire_time = get_uptime_ms();
#endif

	spinlock_release(&mtx->mtx_lock);
	return true;
}

void
mutex_unlock(mutex_t *mtx)
{
	struct task *current_task;
	struct task *next_task;

	if (mtx == NULL) {
		panic("mutex_unlock: NULL mutex pointer");
	}

	current_task = scheduler_get_current_task();
	if (current_task == NULL) {
		/* Early boot or interrupt context */
		if ((mtx->mtx_type & MTX_SPIN) == 0) {
			panic_fmt("mutex_unlock: no current task for mutex '%s'",
			          mutex_name(mtx));
		}
		spinlock_release(&mtx->mtx_lock);
		return;
	}

	spinlock_acquire(&mtx->mtx_lock);

	/* Verify we own the mutex */
	if (mtx->mtx_owner != current_task) {
		spinlock_release(&mtx->mtx_lock);
		panic_fmt("mutex_unlock: mutex '%s' not owned by current task",
		          mutex_name(mtx));
	}

	/* Handle recursive unlocking */
	if (mtx->mtx_recurse > 1) {
		mtx->mtx_recurse--;
		spinlock_release(&mtx->mtx_lock);
		return;
	}

	/* Release the mutex */
	WRITE_ONCE(mtx->mtx_owner, NULL);
	mtx->mtx_recurse = 0;

	/* Wake up next waiting task if any */
	next_task = mtx_waitq_dequeue(mtx);
	if (next_task != NULL) {
		scheduler_unblock_task(next_task);
	}

	spinlock_release(&mtx->mtx_lock);
}

bool
mutex_owned(const mutex_t *mtx)
{
	struct task *current_task;

	if (mtx == NULL) {
		return false;
	}

	current_task = scheduler_get_current_task();
	if (current_task == NULL) {
		return false;
	}

	return READ_ONCE(mtx->mtx_owner) == current_task;
}

bool
mutex_locked(const mutex_t *mtx)
{
	if (mtx == NULL) {
		return false;
	}

	return READ_ONCE(mtx->mtx_owner) != NULL;
}

struct task *
mutex_owner(const mutex_t *mtx)
{
	if (mtx == NULL) {
		return NULL;
	}

	return (struct task *)READ_ONCE(mtx->mtx_owner);
}

#ifdef MTX_DEBUG

void
mutex_assert_locked(const mutex_t *mtx, const char *file, int line)
{
	if (!mutex_locked(mtx)) {
		panic_fmt("%s:%d: mutex '%s' not locked",
		          file, line, mutex_name(mtx));
	}
}

void
mutex_assert_unlocked(const mutex_t *mtx, const char *file, int line)
{
	if (mutex_locked(mtx)) {
		panic_fmt("%s:%d: mutex '%s' is locked",
		          file, line, mutex_name(mtx));
	}
}

void
mutex_assert_owned(const mutex_t *mtx, const char *file, int line)
{
	if (!mutex_owned(mtx)) {
		panic_fmt("%s:%d: mutex '%s' not owned by current task",
		          file, line, mutex_name(mtx));
	}
}

void
mutex_print_stats(const mutex_t *mtx)
{
	struct task *owner;
	uint64_t hold_time;

	if (mtx == NULL) {
		return;
	}

	printf("Mutex '%s' statistics:\n", mutex_name(mtx));
	printf("  Type: %s%s\n",
	       (mtx->mtx_type & MTX_RECURSE) ? "RECURSIVE " : "",
	       (mtx->mtx_type & MTX_SPIN) ? "SPIN" : "SLEEP");

	owner = (struct task *)mtx->mtx_owner;
	if (owner != NULL) {
		printf("  Locked: yes (by task %u: %s)\n",
		       owner->tid, owner->name);
		printf("  Recursion count: %u\n", mtx->mtx_recurse);

		if (mtx->mtx_acquire_time > 0) {
			hold_time = get_uptime_ms() - mtx->mtx_acquire_time;
			printf("  Held for: %llu ms\n",
			       (unsigned long long)hold_time);
		}

		if (mtx->mtx_file != NULL) {
			printf("  Acquired at: %s:%d\n",
			       mtx->mtx_file, mtx->mtx_line);
		}
	} else {
		printf("  Locked: no\n");
	}

	printf("  Contention count: %llu\n",
	       (unsigned long long)mtx->mtx_contention_count);

	if (!mtx_waitq_empty(mtx)) {
		mtx_waitq_t *entry;
		int count = 0;

		printf("  Waiting tasks:\n");
		for (entry = mtx->mtx_waitq_head; entry != NULL; entry = entry->next) {
			count++;
			if (entry->task != NULL) {
				printf("    %d. Task %u: %s\n",
				       count,
				       entry->task->tid,
				       entry->task->name);
			}
		}
	}
}

#endif /* MTX_DEBUG */