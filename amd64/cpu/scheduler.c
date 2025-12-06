#include <scheduler.h>
#include <pit.h>
#include <isr.h>
#include <pmm.h>
#include <vmm.h>
#include <vfs.h>
#include <string.h>
#include <trap.h>

extern void tty_printf(const char *fmt, ...);

#define SCHEDULER_TIME_SLICE_MS 20  // 20ms time slice
#define MAX_TASKS 256
#define KERNEL_STACK_SIZE (16 * 1024)  // 16KB kernel stack
#define USER_STACK_SIZE (64 * 1024)    // 64KB user stack

typedef struct {
    task_t *head;
    task_t *tail;
    uint32_t count;
} task_queue_t;

static struct {
    task_queue_t ready_queues[5];    // One queue per priority level
    task_t *blocked_list;
    task_t *sleeping_list;
    task_t *current_task;
    task_t *idle_task;
    
    uint32_t next_tid;
    uint32_t timer_frequency;
    uint64_t time_slice_ticks;
    bool running;                    // Scheduler enabled
    
    scheduler_stats_t stats;
} scheduler;

static void scheduler_switch_to_next(void);
static task_t *scheduler_pick_next_task(void);
static void queue_add(task_queue_t *queue, task_t *task);
static void queue_remove(task_queue_t *queue, task_t *task);
static void list_add(task_t **list, task_t *task);
static void list_remove(task_t **list, task_t *task);
static void idle_task_entry(void);
static void scheduler_timer_handler(registers_t *regs);

void scheduler_init(uint32_t timer_frequency) {
    memset(&scheduler, 0, sizeof(scheduler));
    
    for (int i = 0; i < 5; i++) {
        scheduler.ready_queues[i].head = NULL;
        scheduler.ready_queues[i].tail = NULL;
        scheduler.ready_queues[i].count = 0;
    }
    
    scheduler.blocked_list = NULL;
    scheduler.sleeping_list = NULL;
    scheduler.current_task = NULL;
    scheduler.next_tid = 1;
    scheduler.timer_frequency = timer_frequency;
    scheduler.running = false;
    
    scheduler.time_slice_ticks = (SCHEDULER_TIME_SLICE_MS * timer_frequency) / 1000;
    
    scheduler.idle_task = scheduler_create_task("idle", idle_task_entry, 
                                                TASK_PRIORITY_IDLE, true);
    
    isr_register_handler(T_IRQ0, scheduler_timer_handler);
    
    tty_printf("[SCHED] Scheduler initialized (freq=%dHz, slice=%dms)\n", 
               timer_frequency, SCHEDULER_TIME_SLICE_MS);
}

task_t *scheduler_create_task(const char *name, void (*entry_point)(void), 
                               task_priority_t priority, bool is_kernel) {
    task_t *task = pmm_alloc();
    if (!task) return NULL;
    
    memset(task, 0, sizeof(task_t));
    
    task->tid = scheduler.next_tid++;
    strncpy(task->name, name, sizeof(task->name) - 1);
    task->state = TASK_STATE_READY;
    task->priority = priority;
    task->time_slice = scheduler.time_slice_ticks;
    
    task->kernel_stack_size = KERNEL_STACK_SIZE;
    task->kernel_stack = pmm_alloc_contiguous(KERNEL_STACK_SIZE / PAGE_SIZE);
    if (!task->kernel_stack) {
        pmm_free(task);
        return NULL;
    }
    
    if (!is_kernel) {
        task->user_stack_size = USER_STACK_SIZE;
        task->user_stack = pmm_alloc_contiguous(USER_STACK_SIZE / PAGE_SIZE);
        if (!task->user_stack) {
            pmm_free_contiguous(task->kernel_stack, KERNEL_STACK_SIZE / PAGE_SIZE);
            pmm_free(task);
            return NULL;
        }
    }
    
    memset(&task->cpu_state, 0, sizeof(cpu_state_t));
    task->cpu_state.rip = (uint64_t)entry_point;
    task->cpu_state.rflags = 0x202;
    task->cpu_state.cs = 0x08;
    task->cpu_state.ss = 0x10;
    
    task->cpu_state.rsp = (uint64_t)task->kernel_stack + KERNEL_STACK_SIZE - 16;
    task->cpu_state.rbp = task->cpu_state.rsp;
    
    if (is_kernel) {
        task->page_directory = vmm_get_kernel_space();
    } else {
        task->page_directory = vmm_create_address_space(false);
    }
    
    if (task->page_directory) {
        vmm_address_space_t *space = (vmm_address_space_t *)task->page_directory;
        task->cpu_state.cr3 = (uint64_t)space->page_dir;
    }

    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        task->fd_table[i] = NULL;
    }
    
    if (task != scheduler.idle_task) {
        queue_add(&scheduler.ready_queues[priority], task);
        scheduler.stats.total_tasks++;
        scheduler.stats.ready_tasks++;
    }
    
    tty_printf("[SCHED] Created task %d: %s (priority=%d)\n", 
               task->tid, task->name, priority);
    
    return task;
}

void scheduler_destroy_task(task_t *task) {
    if (!task || task == scheduler.idle_task) return;

    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (task->fd_table[i] != NULL) {
            vfs_close((vfs_file_t *)task->fd_table[i]);
            task->fd_table[i] = NULL;
        }
    }
    
    switch (task->state) {
        case TASK_STATE_READY:
            queue_remove(&scheduler.ready_queues[task->priority], task);
            scheduler.stats.ready_tasks--;
            break;
        case TASK_STATE_BLOCKED:
            list_remove(&scheduler.blocked_list, task);
            scheduler.stats.blocked_tasks--;
            break;
        case TASK_STATE_SLEEPING:
            list_remove(&scheduler.sleeping_list, task);
            break;
        default:
            break;
    }
    
    if (task->kernel_stack) {
        pmm_free_contiguous(task->kernel_stack, KERNEL_STACK_SIZE / PAGE_SIZE);
    }
    if (task->user_stack) {
        pmm_free_contiguous(task->user_stack, USER_STACK_SIZE / PAGE_SIZE);
    }
    
    vmm_address_space_t *kernel_space = vmm_get_kernel_space();
    if (task->page_directory && task->page_directory != kernel_space) {
        vmm_destroy_address_space((vmm_address_space_t *)task->page_directory);
    }
    
    pmm_free(task);
    scheduler.stats.total_tasks--;
}

void scheduler_exit_task(uint32_t exit_code) {
    if (!scheduler.current_task) return;
    
    task_t *task = scheduler.current_task;
    task->state = TASK_STATE_TERMINATED;
    task->exit_code = exit_code;
    
    tty_printf("[SCHED] Task %d (%s) exited with code %d\n", 
               task->tid, task->name, exit_code);
    
    scheduler.current_task = NULL;
    scheduler_switch_to_next();
}

void scheduler_block_task(task_t *task) {
    if (!task || task->state == TASK_STATE_BLOCKED) return;
    
    if (task->state == TASK_STATE_READY) {
        queue_remove(&scheduler.ready_queues[task->priority], task);
        scheduler.stats.ready_tasks--;
    }
    
    task->state = TASK_STATE_BLOCKED;
    list_add(&scheduler.blocked_list, task);
    scheduler.stats.blocked_tasks++;
    
    if (task == scheduler.current_task) {
        scheduler_yield();
    }
}

void scheduler_unblock_task(task_t *task) {
    if (!task || task->state != TASK_STATE_BLOCKED) return;
    
    list_remove(&scheduler.blocked_list, task);
    scheduler.stats.blocked_tasks--;
    
    task->state = TASK_STATE_READY;
    task->time_used = 0;  // Reset time slice
    queue_add(&scheduler.ready_queues[task->priority], task);
    scheduler.stats.ready_tasks++;
}

void scheduler_sleep_task(task_t *task, uint64_t milliseconds) {
    if (!task) return;
    
    uint64_t current_time = (pit_get_ticks() * 1000) / pit_get_frequency();
    task->sleep_until = current_time + milliseconds;
    
    if (task->state == TASK_STATE_READY) {
        queue_remove(&scheduler.ready_queues[task->priority], task);
        scheduler.stats.ready_tasks--;
    }
    
    task->state = TASK_STATE_SLEEPING;
    list_add(&scheduler.sleeping_list, task);
    
    // If sleeping current task, switch
    if (task == scheduler.current_task) {
        scheduler_yield();
    }
}

void scheduler_yield(void) {
    if (!scheduler.running || !scheduler.current_task) return;
    scheduler_switch_to_next();
}

void scheduler_start(void) {
    if (scheduler.running) return;
    
    scheduler.running = true;
    tty_printf("[SCHED] Scheduler started\n");
    
    pit_start();
    
    task_t *first_task = scheduler_pick_next_task();
    if (first_task) {
        scheduler.current_task = first_task;
        first_task->state = TASK_STATE_RUNNING;
        scheduler.stats.running_tasks = 1;
        
        vmm_switch_address_space((vmm_address_space_t *)first_task->page_directory);
        scheduler_switch_context(NULL, &first_task->cpu_state);
    }
}

void scheduler_stop(void) {
    scheduler.running = false;
    pit_stop();
    tty_printf("[SCHED] Scheduler stopped\n");
}

void scheduler_tick(void) {
    scheduler.stats.total_ticks++;
    
    uint64_t current_time = (pit_get_ticks() * 1000) / pit_get_frequency();
    task_t *task = scheduler.sleeping_list;
    while (task) {
        task_t *next = task->next;
        if (current_time >= task->sleep_until) {
            list_remove(&scheduler.sleeping_list, task);
            task->state = TASK_STATE_READY;
            task->time_used = 0;
            queue_add(&scheduler.ready_queues[task->priority], task);
            scheduler.stats.ready_tasks++;
        }
        task = next;
    }
    
    // Check if current task's time slice expired
    if (scheduler.current_task) {
        scheduler.current_task->time_used++;
        scheduler.current_task->total_time++;
        
        if (scheduler.current_task->time_used >= scheduler.current_task->time_slice) {
            scheduler_switch_to_next();
        }
    }
}

task_t *scheduler_get_current_task(void) {
    return scheduler.current_task;
}

task_t *scheduler_get_task_by_tid(uint32_t tid) {
    for (int i = 0; i < 5; i++) {
        task_t *task = scheduler.ready_queues[i].head;
        while (task) {
            if (task->tid == tid) return task;
            task = task->next;
        }
    }
    
    task_t *task = scheduler.blocked_list;
    while (task) {
        if (task->tid == tid) return task;
        task = task->next;
    }
    
    task = scheduler.sleeping_list;
    while (task) {
        if (task->tid == tid) return task;
        task = task->next;
    }
    
    if (scheduler.current_task && scheduler.current_task->tid == tid) {
        return scheduler.current_task;
    }
    
    return NULL;
}

void scheduler_set_priority(task_t *task, task_priority_t priority) {
    if (!task || task->priority == priority) return;
    
    // If task is ready, move to new priority queue
    if (task->state == TASK_STATE_READY) {
        queue_remove(&scheduler.ready_queues[task->priority], task);
        task->priority = priority;
        queue_add(&scheduler.ready_queues[priority], task);
    } else {
        task->priority = priority;
    }
}

task_priority_t scheduler_get_priority(task_t *task) {
    return task ? task->priority : TASK_PRIORITY_IDLE;
}

scheduler_stats_t scheduler_get_stats(void) {
    return scheduler.stats;
}

void scheduler_dump_tasks(void) {
    tty_printf("\n=== Scheduler State ===\n");
    tty_printf("Running: %s\n", scheduler.running ? "Yes" : "No");
    tty_printf("Total tasks: %llu\n", scheduler.stats.total_tasks);
    tty_printf("Context switches: %llu\n", scheduler.stats.context_switches);
    
    tty_printf("\nCurrent task:\n");
    if (scheduler.current_task) {
        scheduler_print_task(scheduler.current_task);
    } else {
        tty_printf("  (none)\n");
    }
    
    tty_printf("\nReady queues:\n");
    for (int i = 4; i >= 0; i--) {
        if (scheduler.ready_queues[i].count > 0) {
            tty_printf("  Priority %d (%d tasks):\n", i, scheduler.ready_queues[i].count);
            task_t *task = scheduler.ready_queues[i].head;
            while (task) {
                tty_printf("    ");
                scheduler_print_task(task);
                task = task->next;
            }
        }
    }
}

void scheduler_print_task(task_t *task) {
    if (!task) return;
    
    const char *state_str[] = {"READY", "RUNNING", "BLOCKED", "SLEEPING", "TERMINATED"};
    tty_printf("Task %d: %s [%s] pri=%d time=%llu\n",
               task->tid, task->name, state_str[task->state],
               task->priority, task->total_time);
}

static void scheduler_switch_to_next(void) {
    if (!scheduler.running) return;
    
    task_t *old_task = scheduler.current_task;
    task_t *new_task = scheduler_pick_next_task();
    
    if (!new_task) {
        new_task = scheduler.idle_task;
    }
    
    if (old_task == new_task) {
        if (old_task) old_task->time_used = 0;
        return;
    }
    
    if (old_task && old_task->state == TASK_STATE_RUNNING) {
        old_task->state = TASK_STATE_READY;
        old_task->time_used = 0;
        queue_add(&scheduler.ready_queues[old_task->priority], old_task);
        scheduler.stats.ready_tasks++;
    }
    
    if (new_task->state == TASK_STATE_READY) {
        queue_remove(&scheduler.ready_queues[new_task->priority], new_task);
        scheduler.stats.ready_tasks--;
    }
    new_task->state = TASK_STATE_RUNNING;
    new_task->time_used = 0;
    
    scheduler.current_task = new_task;
    scheduler.stats.context_switches++;
    
    vmm_switch_address_space((vmm_address_space_t *)new_task->page_directory);
    
    if (old_task) {
        scheduler_switch_context(&old_task->cpu_state, &new_task->cpu_state);
    } else {
        scheduler_switch_context(NULL, &new_task->cpu_state);
    }
}

static task_t *scheduler_pick_next_task(void) {
    for (int i = 4; i >= 0; i--) {
        if (scheduler.ready_queues[i].head) {
            return scheduler.ready_queues[i].head;
        }
    }
    return NULL;
}

static void queue_add(task_queue_t *queue, task_t *task) {
    task->next = NULL;
    task->prev = queue->tail;
    
    if (queue->tail) {
        queue->tail->next = task;
    } else {
        queue->head = task;
    }
    queue->tail = task;
    queue->count++;
}

static void queue_remove(task_queue_t *queue, task_t *task) {
    if (task->prev) {
        task->prev->next = task->next;
    } else {
        queue->head = task->next;
    }
    
    if (task->next) {
        task->next->prev = task->prev;
    } else {
        queue->tail = task->prev;
    }
    
    task->next = NULL;
    task->prev = NULL;
    queue->count--;
}

static void list_add(task_t **list, task_t *task) {
    task->next = *list;
    task->prev = NULL;
    if (*list) {
        (*list)->prev = task;
    }
    *list = task;
}

static void list_remove(task_t **list, task_t *task) {
    if (task->prev) {
        task->prev->next = task->next;
    } else {
        *list = task->next;
    }
    
    if (task->next) {
        task->next->prev = task->prev;
    }
    
    task->next = NULL;
    task->prev = NULL;
}

static void idle_task_entry(void) {
    while (1) {
        __asm__ volatile("hlt");
    }
}

static void scheduler_timer_handler(registers_t *regs) {
    (void)regs;
    
    pit_handler();
    
    scheduler_tick();
}