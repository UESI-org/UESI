#ifndef _SYS_MUTEX_H_
#define _SYS_MUTEX_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/spinlock.h>

#include <stdbool.h>

__BEGIN_DECLS

struct task;

#define MTX_DEF		0x00000000	/* Default (non-recursive) mutex */
#define MTX_RECURSE	0x00000001	/* Recursive mutex */
#define MTX_SPIN	0x00000002	/* Spin mutex (no sleep) */

typedef struct mtx_waitq {
	struct task *task;
	struct mtx_waitq *next;
} mtx_waitq_t;

typedef struct mutex {
	spinlock_t mtx_lock;		/* Spinlock for internal state */
	volatile struct task *mtx_owner;	/* Current owner task */
	volatile unsigned int mtx_recurse;	/* Recursion count */
	unsigned int mtx_type;		/* Mutex type flags */
	const char *mtx_name;		/* Mutex name for debugging */
	mtx_waitq_t *mtx_waitq_head;	/* Head of wait queue */
	mtx_waitq_t *mtx_waitq_tail;	/* Tail of wait queue */
	
#ifdef MTX_DEBUG
	const char *mtx_file;		/* File where last acquired */
	int mtx_line;			/* Line where last acquired */
	uint64_t mtx_acquire_time;	/* Time of acquisition */
	uint64_t mtx_contention_count;	/* Number of times contended */
#endif
} mutex_t;

#ifdef MTX_DEBUG
#define MUTEX_INITIALIZER(name, type) { \
	.mtx_lock = SPINLOCK_INITIALIZER("mtx:" name), \
	.mtx_owner = NULL, \
	.mtx_recurse = 0, \
	.mtx_type = (type), \
	.mtx_name = (name), \
	.mtx_waitq_head = NULL, \
	.mtx_waitq_tail = NULL, \
	.mtx_file = NULL, \
	.mtx_line = 0, \
	.mtx_acquire_time = 0, \
	.mtx_contention_count = 0 \
}
#else
#define MUTEX_INITIALIZER(name, type) { \
	.mtx_lock = SPINLOCK_INITIALIZER("mtx:" name), \
	.mtx_owner = NULL, \
	.mtx_recurse = 0, \
	.mtx_type = (type), \
	.mtx_name = (name), \
	.mtx_waitq_head = NULL, \
	.mtx_waitq_tail = NULL \
}
#endif

void mutex_init(mutex_t *mtx, const char *name, unsigned int type);
void mutex_destroy(mutex_t *mtx);

void mutex_lock(mutex_t *mtx);
bool mutex_trylock(mutex_t *mtx);
void mutex_unlock(mutex_t *mtx);

bool mutex_owned(const mutex_t *mtx);
bool mutex_locked(const mutex_t *mtx);

struct task *mutex_owner(const mutex_t *mtx);

#ifdef MTX_DEBUG
void mutex_assert_locked(const mutex_t *mtx, const char *file, int line);
void mutex_assert_unlocked(const mutex_t *mtx, const char *file, int line);
void mutex_assert_owned(const mutex_t *mtx, const char *file, int line);

#define MTX_ASSERT_LOCKED(mtx) \
	mutex_assert_locked((mtx), __FILE__, __LINE__)
#define MTX_ASSERT_UNLOCKED(mtx) \
	mutex_assert_unlocked((mtx), __FILE__, __LINE__)
#define MTX_ASSERT_OWNED(mtx) \
	mutex_assert_owned((mtx), __FILE__, __LINE__)

void mutex_print_stats(const mutex_t *mtx);
#else
#define MTX_ASSERT_LOCKED(mtx)		((void)0)
#define MTX_ASSERT_UNLOCKED(mtx)	((void)0)
#define MTX_ASSERT_OWNED(mtx)		((void)0)
#endif

#define mutex_enter(mtx)	mutex_lock(mtx)
#define mutex_exit(mtx)		mutex_unlock(mtx)

static inline const char *
mutex_name(const mutex_t *mtx)
{
	return mtx->mtx_name ? mtx->mtx_name : "(unnamed)";
}

static inline unsigned int
mutex_type(const mutex_t *mtx)
{
	return mtx->mtx_type;
}

static inline bool
mutex_recursive(const mutex_t *mtx)
{
	return (mtx->mtx_type & MTX_RECURSE) != 0;
}

__END_DECLS

#endif