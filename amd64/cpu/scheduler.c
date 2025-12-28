#include <scheduler.h>
#include <proc.h>
#include <pit.h>
#include <isr.h>
#include <pmm.h>
#include <paging.h>
#include <kmalloc.h>
#include <string.h>
#include <trap.h>
#include <intr.h>
#include <printf.h>

extern void tty_printf(const char *fmt, ...);
extern void tss_set_rsp0(uint64_t rsp);

#define SCHEDULER_TIME_SLICE_MS 20
#define MAX_TASKS 256

typedef struct {
	struct proclist queues[5]; /* One queue per priority level */
	struct proclist blocked_list;
	struct proclist sleeping_list;
	struct proclist terminated_list;
	
	struct proc *idle_task;
	struct process *idle_process;

	uint32_t timer_frequency;
	uint64_t time_slice_ticks;
	bool running;

	scheduler_stats_t stats;
} scheduler_data_t;

static scheduler_data_t scheduler;

static void scheduler_switch_to_next(void);
static struct proc *scheduler_pick_next_task(void);
static void idle_task_entry(void);
static void scheduler_timer_handler(registers_t *regs);

static inline task_priority_t
proc_get_priority(struct proc *p)
{
	return (task_priority_t)p->p_runpri;
}

static inline void
proc_set_priority(struct proc *p, task_priority_t priority)
{
	p->p_runpri = (uint8_t)priority;
}

static inline task_state_t
proc_to_task_state(struct proc *p)
{
	switch (p->p_stat) {
	case SRUN:
		return TASK_STATE_READY;
	case SONPROC:
		return TASK_STATE_RUNNING;
	case SSTOP:
		return TASK_STATE_BLOCKED;
	case SSLEEP:
		return TASK_STATE_SLEEPING;
	case SDEAD:
		return TASK_STATE_TERMINATED;
	default:
		return TASK_STATE_READY;
	}
}

void
scheduler_init(uint32_t timer_frequency)
{
	memset(&scheduler, 0, sizeof(scheduler));

	for (int i = 0; i < 5; i++) {
		TAILQ_INIT(&scheduler.queues[i]);
	}
	TAILQ_INIT(&scheduler.blocked_list);
	TAILQ_INIT(&scheduler.sleeping_list);
	TAILQ_INIT(&scheduler.terminated_list);

	scheduler.timer_frequency = timer_frequency;
	scheduler.running = false;
	scheduler.time_slice_ticks =
	    (SCHEDULER_TIME_SLICE_MS * timer_frequency) / 1000;

	proc_init();

	scheduler.idle_process = process_alloc("idle");
	if (scheduler.idle_process) {
		scheduler.idle_task = proc_alloc(scheduler.idle_process, "idle");
		if (scheduler.idle_task) {
			proc_set_priority(scheduler.idle_task, TASK_PRIORITY_IDLE);
		}
	}

	isr_register_handler(T_IRQ0, scheduler_timer_handler);

	tty_printf("[SCHED] Scheduler initialized (freq=%dHz, slice=%dms)\n",
	           timer_frequency,
	           SCHEDULER_TIME_SLICE_MS);
}

task_t *
scheduler_create_task(const char *name,
                      void (*entry_point)(void),
                      task_priority_t priority,
                      bool is_kernel)
{
	struct process *ps;
	struct proc *p;

	ps = process_alloc(name);
	if (!ps)
		return NULL;

	p = proc_alloc(ps, name);
	if (!p) {
		process_free(ps);
		return NULL;
	}

	proc_set_priority(p, priority);

	/* Store entry point in machine-dependent structure */
	/* This would need to be stored in trapframe or similar */
	/* For now, we'll assume the entry point gets set elsewhere */
	(void)entry_point; /* TODO: Store entry point properly */

	p->p_stat = SRUN;

	if (p != scheduler.idle_task) {
		TAILQ_INSERT_TAIL(&scheduler.queues[priority], p, p_runq);
		scheduler.stats.total_tasks++;
		scheduler.stats.ready_tasks++;
	}

	tty_printf("[SCHED] Created task %d: %s (priority=%d)\n",
	           p->p_tid,
	           name,
	           priority);

	return p;
}

void
scheduler_destroy_task(task_t *task)
{
	struct proc *p = task;

	if (!p || p == scheduler.idle_task)
		return;

	task_state_t state = proc_to_task_state(p);
	switch (state) {
	case TASK_STATE_READY:
		TAILQ_REMOVE(&scheduler.queues[proc_get_priority(p)], p, p_runq);
		scheduler.stats.ready_tasks--;
		break;
	case TASK_STATE_BLOCKED:
		TAILQ_REMOVE(&scheduler.blocked_list, p, p_runq);
		scheduler.stats.blocked_tasks--;
		break;
	case TASK_STATE_SLEEPING:
		TAILQ_REMOVE(&scheduler.sleeping_list, p, p_runq);
		break;
	default:
		break;
	}

	scheduler.stats.total_tasks--;

	struct process *ps = p->p_p;
	proc_free(p);
	if (ps && ps->ps_threadcnt == 0) {
		process_free(ps);
	}
}

void
scheduler_exit_task(uint32_t exit_code)
{
	struct proc *p = proc_get_current();
	if (!p)
		return;

	struct process *ps = p->p_p;

	p->p_stat = SDEAD;
	ps->ps_xexit = exit_code;

	tty_printf("[SCHED] Task %d (%s) exited with code %d\n",
	           p->p_tid,
	           p->p_name,
	           exit_code);

	TAILQ_INSERT_TAIL(&scheduler.terminated_list, p, p_runq);

	proc_set_current(NULL);
	scheduler_switch_to_next();
}

void
scheduler_block_task(task_t *task)
{
	struct proc *p = task;

	if (!p || p->p_stat == SSTOP)
		return;

	if (p->p_stat == SRUN) {
		TAILQ_REMOVE(&scheduler.queues[proc_get_priority(p)], p, p_runq);
		scheduler.stats.ready_tasks--;
	}

	p->p_stat = SSTOP;
	TAILQ_INSERT_TAIL(&scheduler.blocked_list, p, p_runq);
	scheduler.stats.blocked_tasks++;

	if (p == proc_get_current()) {
		scheduler_yield();
	}
}

void
scheduler_unblock_task(task_t *task)
{
	struct proc *p = task;

	if (!p || p->p_stat != SSTOP)
		return;

	TAILQ_REMOVE(&scheduler.blocked_list, p, p_runq);
	scheduler.stats.blocked_tasks--;

	p->p_stat = SRUN;
	p->p_cpticks = 0;
	TAILQ_INSERT_TAIL(&scheduler.queues[proc_get_priority(p)], p, p_runq);
	scheduler.stats.ready_tasks++;
}

void
scheduler_sleep_task(task_t *task, uint64_t milliseconds)
{
	struct proc *p = task;

	if (!p)
		return;

	uint64_t current_time = (pit_get_ticks() * 1000) / pit_get_frequency();
	p->p_slptime = current_time + milliseconds;

	if (p->p_stat == SRUN) {
		TAILQ_REMOVE(&scheduler.queues[proc_get_priority(p)], p, p_runq);
		scheduler.stats.ready_tasks--;
	}

	p->p_stat = SSLEEP;
	TAILQ_INSERT_TAIL(&scheduler.sleeping_list, p, p_runq);

	if (p == proc_get_current()) {
		scheduler_yield();
	}
}

void
scheduler_yield(void)
{
	if (!scheduler.running || !proc_get_current())
		return;
	scheduler_switch_to_next();
}

void
scheduler_start(void)
{
	if (scheduler.running)
		return;

	scheduler.running = true;
	tty_printf("[SCHED] Scheduler started\n");

	pit_start();

	struct proc *first_task = scheduler_pick_next_task();
	if (!first_task) {
		first_task = scheduler.idle_task;
	}

	if (first_task) {
		proc_set_current(first_task);
		first_task->p_stat = SONPROC;
		scheduler.stats.running_tasks = 1;

		if (first_task->p_vmspace) {
			/* Load CR3 with physical address */
			uint64_t cr3 = first_task->p_vmspace->phys_addr;
			__asm__ volatile("movq %0, %%cr3" : : "r"(cr3));
		}

		/* Context switch would happen here */
		/* This is architecture-specific */
	}
}

void
scheduler_stop(void)
{
	scheduler.running = false;
	pit_stop();
	tty_printf("[SCHED] Scheduler stopped\n");
}

void
scheduler_tick(void)
{
	scheduler.stats.total_ticks++;

	struct proc *p = TAILQ_FIRST(&scheduler.terminated_list);
	while (p) {
		struct proc *next = TAILQ_NEXT(p, p_runq);
		TAILQ_REMOVE(&scheduler.terminated_list, p, p_runq);
		scheduler_destroy_task(p);
		p = next;
	}

	uint64_t current_time = (pit_get_ticks() * 1000) / pit_get_frequency();
	p = TAILQ_FIRST(&scheduler.sleeping_list);
	while (p) {
		struct proc *next = TAILQ_NEXT(p, p_runq);
		if (current_time >= p->p_slptime) {
			TAILQ_REMOVE(&scheduler.sleeping_list, p, p_runq);
			p->p_stat = SRUN;
			p->p_cpticks = 0;
			TAILQ_INSERT_TAIL(&scheduler.queues[proc_get_priority(p)],
			                  p,
			                  p_runq);
			scheduler.stats.ready_tasks++;
		}
		p = next;
	}

	struct proc *current = proc_get_current();
	if (current && current != scheduler.idle_task) {
		current->p_cpticks++;
		current->p_tu.tu_runtime++;

		if (current->p_cpticks >= scheduler.time_slice_ticks) {
			scheduler_switch_to_next();
		}
	}
}

task_t *
scheduler_get_current_task(void)
{
	return proc_get_current();
}

task_t *
scheduler_add_forked_task(task_t *task)
{
	struct proc *p = task;

	if (!p)
		return NULL;

	TAILQ_INSERT_TAIL(&scheduler.queues[proc_get_priority(p)], p, p_runq);
	scheduler.stats.total_tasks++;
	scheduler.stats.ready_tasks++;

	tty_printf("[SCHED] Added forked task %d: %s to ready queue\n",
	           p->p_tid,
	           p->p_name);

	return p;
}

task_t *
scheduler_get_task_by_tid(uint32_t tid)
{
	return tfind(tid);
}

void
scheduler_set_priority(task_t *task, task_priority_t priority)
{
	struct proc *p = task;

	if (!p || proc_get_priority(p) == priority)
		return;

	if (p->p_stat == SRUN) {
		TAILQ_REMOVE(&scheduler.queues[proc_get_priority(p)], p, p_runq);
		proc_set_priority(p, priority);
		TAILQ_INSERT_TAIL(&scheduler.queues[priority], p, p_runq);
	} else {
		proc_set_priority(p, priority);
	}
}

task_priority_t
scheduler_get_priority(task_t *task)
{
	struct proc *p = task;
	return p ? proc_get_priority(p) : TASK_PRIORITY_IDLE;
}

scheduler_stats_t
scheduler_get_stats(void)
{
	return scheduler.stats;
}

void
scheduler_dump_tasks(void)
{
	tty_printf("\n=== Scheduler State ===\n");
	tty_printf("Running: %s\n", scheduler.running ? "Yes" : "No");
	tty_printf("Total tasks: %llu\n", scheduler.stats.total_tasks);
	tty_printf("Context switches: %llu\n",
	           scheduler.stats.context_switches);

	tty_printf("\nCurrent task:\n");
	struct proc *current = proc_get_current();
	if (current) {
		scheduler_print_task(current);
	} else {
		tty_printf("  (none)\n");
	}

	tty_printf("\nReady queues:\n");
	for (int i = 4; i >= 0; i--) {
		struct proc *p;
		int count = 0;
		TAILQ_FOREACH(p, &scheduler.queues[i], p_runq) {
			count++;
		}
		if (count > 0) {
			tty_printf("  Priority %d (%d tasks):\n", i, count);
			TAILQ_FOREACH(p, &scheduler.queues[i], p_runq) {
				tty_printf("    ");
				scheduler_print_task(p);
			}
		}
	}
}

void
scheduler_print_task(task_t *task)
{
	struct proc *p = task;

	if (!p)
		return;

	const char *state_str[] = {
		"UNKNOWN", "IDLE", "READY", "SLEEPING", "STOPPED",
		"UNKNOWN", "DEAD", "RUNNING"
	};
	
	tty_printf("Task %d: %s [%s] pri=%d time=%llu\n",
	           p->p_tid,
	           p->p_name,
	           state_str[(int)p->p_stat],
	           proc_get_priority(p),
	           p->p_tu.tu_runtime);
}

static void
scheduler_switch_to_next(void)
{
	if (!scheduler.running)
		return;

	uint64_t flags = intr_disable();

	struct proc *old_task = proc_get_current();
	struct proc *new_task = scheduler_pick_next_task();

	if (!new_task) {
		new_task = scheduler.idle_task;
	}

	if (old_task == new_task) {
		if (old_task)
			old_task->p_cpticks = 0;
		intr_restore(flags);
		return;
	}

	tty_printf("[SCHED] Switching from task %d to task %d\n",
	           old_task ? old_task->p_tid : -1,
	           new_task ? new_task->p_tid : -1);

	if (old_task && old_task->p_stat == SONPROC) {
		old_task->p_stat = SRUN;
		old_task->p_cpticks = 0;
		TAILQ_INSERT_TAIL(&scheduler.queues[proc_get_priority(old_task)],
		                  old_task,
		                  p_runq);
		scheduler.stats.ready_tasks++;
		scheduler.stats.running_tasks--;
	}

	if (new_task->p_stat == SRUN) {
		TAILQ_REMOVE(&scheduler.queues[proc_get_priority(new_task)],
		             new_task,
		             p_runq);
		scheduler.stats.ready_tasks--;
	}
	new_task->p_stat = SONPROC;
	new_task->p_cpticks = 0;
	scheduler.stats.running_tasks++;

	proc_set_current(new_task);
	scheduler.stats.context_switches++;

	if (new_task->p_vmspace) {
		uint64_t cr3 = new_task->p_vmspace->phys_addr;
		__asm__ volatile("movq %0, %%cr3" : : "r"(cr3));
	}

	if (old_task && old_task->p_md.md_cpu_state && 
	    new_task->p_md.md_cpu_state) {
		
		tty_printf("[SCHED] Context switching with cpu_state\n");
		
		cpu_state_t *old_state = (cpu_state_t *)old_task->p_md.md_cpu_state;
		cpu_state_t *new_state = (cpu_state_t *)new_task->p_md.md_cpu_state;
		
		scheduler_switch_context(old_state, new_state);
		
		intr_restore(flags);
		return;
	}
	
	if (!old_task || !old_task->p_md.md_cpu_state) {
		if (new_task->p_md.md_cpu_state) {
			tty_printf("[SCHED] Initial switch to task %d\n", new_task->p_tid);
			
			cpu_state_t *new_state = (cpu_state_t *)new_task->p_md.md_cpu_state;
			
			scheduler_switch_context(NULL, new_state);
			
			__builtin_unreachable();
		}
	}

	tty_printf("[SCHED] WARNING: No context switch performed!\n");
	intr_restore(flags);
}

static struct proc *
scheduler_pick_next_task(void)
{
	for (int i = 4; i >= 0; i--) {
		struct proc *p = TAILQ_FIRST(&scheduler.queues[i]);
		if (p)
			return p;
	}
	return NULL;
}

static void
idle_task_entry(void)
{
	while (1) {
		__asm__ volatile("hlt");
	}
}

static void
scheduler_timer_handler(registers_t *regs)
{
	(void)regs;

	pit_handler();
	scheduler_tick();
}