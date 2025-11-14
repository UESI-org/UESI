#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "types.h"
#include "paging.h"

#define PROCESS_MAX_NAME 64
#define PROCESS_KERNEL_STACK_SIZE (16 * 1024)  /* 16KB kernel stack */
#define PROCESS_USER_STACK_SIZE   (2 * 1024 * 1024)  /* 2MB user stack */

#define USER_STACK_TOP    0x00007FFFFFFFF000ULL
#define USER_CODE_BASE    0x0000000000400000ULL

typedef enum {
    PROCESS_STATE_READY,
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_BLOCKED,
    PROCESS_STATE_ZOMBIE,
    PROCESS_STATE_DEAD
} process_state_t;

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} process_context_t;

typedef struct process {
    pid_t pid;
    char name[PROCESS_MAX_NAME];
    process_state_t state;
    
    /* Memory management */
    page_directory_t *page_dir;
    uint64_t brk;  /* Program break for heap */
    
    void *kernel_stack;
    uint64_t kernel_stack_top;
    uint64_t user_stack_top;
    
    process_context_t context;
    
    /* Accounting */
    int exit_code;
    uint64_t time_slice;
    uint64_t cpu_time;
    
    /* Linked list */
    struct process *next;
    struct process *prev;
} process_t;

void process_init(void);
process_t *process_create(const char *name);
void process_destroy(process_t *proc);
process_t *process_get_current(void);
void process_set_current(process_t *proc);

void process_enter_usermode(process_t *proc, uint64_t entry_point, 
                           uint64_t stack_top) __attribute__((noreturn));

pid_t process_alloc_pid(void);
void process_switch_to(process_t *next);

#endif