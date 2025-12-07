#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum {
    TASK_STATE_READY,
    TASK_STATE_RUNNING,
    TASK_STATE_BLOCKED,
    TASK_STATE_SLEEPING,
    TASK_STATE_TERMINATED
} task_state_t;

typedef enum {
    TASK_PRIORITY_IDLE = 0,
    TASK_PRIORITY_LOW = 1,
    TASK_PRIORITY_NORMAL = 2,
    TASK_PRIORITY_HIGH = 3,
    TASK_PRIORITY_REALTIME = 4
} task_priority_t;

// CPU register state for context switching
typedef struct {
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi, rbp, rsp;
    uint64_t r8, r9, r10, r11;
    uint64_t r12, r13, r14, r15;
    uint64_t rip;
    uint64_t rflags;
    uint64_t cs, ss;
    uint64_t cr3;  // Page directory base
} cpu_state_t;

// Task Control Block (TCB)
typedef struct task {
    uint32_t tid;
    char name[64];
    task_state_t state;
    task_priority_t priority;
    
    cpu_state_t cpu_state;
    void *kernel_stack;
    void *user_stack;
    size_t kernel_stack_size;
    size_t user_stack_size;
    
    void *page_directory;
    
    uint64_t time_slice;
    uint64_t time_used;
    uint64_t total_time;
    uint64_t sleep_until;
    
    uint32_t exit_code;
    
    #define MAX_OPEN_FILES 32
    void *fd_table[MAX_OPEN_FILES];
    
    struct task *next;
    struct task *prev;
} task_t;

typedef struct {
    uint64_t total_tasks;
    uint64_t ready_tasks;
    uint64_t running_tasks;
    uint64_t blocked_tasks;
    uint64_t context_switches;
    uint64_t total_ticks;
} scheduler_stats_t;

void scheduler_init(uint32_t timer_frequency);

task_t *scheduler_create_task(const char *name, void (*entry_point)(void), 
                               task_priority_t priority, bool is_kernel);
task_t *scheduler_add_forked_task(task_t *task);
void scheduler_destroy_task(task_t *task);
void scheduler_exit_task(uint32_t exit_code);

void scheduler_block_task(task_t *task);
void scheduler_unblock_task(task_t *task);
void scheduler_sleep_task(task_t *task, uint64_t milliseconds);
void scheduler_yield(void);

void scheduler_start(void);
void scheduler_stop(void);
void scheduler_tick(void);
task_t *scheduler_get_current_task(void);
task_t *scheduler_get_task_by_tid(uint32_t tid);

void scheduler_set_priority(task_t *task, task_priority_t priority);
task_priority_t scheduler_get_priority(task_t *task);

scheduler_stats_t scheduler_get_stats(void);
void scheduler_dump_tasks(void);
void scheduler_print_task(task_t *task);

extern void scheduler_switch_context(cpu_state_t *old_state, cpu_state_t *new_state);

#endif