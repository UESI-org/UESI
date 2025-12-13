#ifndef _SYS_PROC_H_
#define _SYS_PROC_H_

#include <mproc.h>
#include <sys/queue.h>
#include <sys/syslimits.h>
#include <sys/atomic.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <paging.h>

#define PID_MAX 99999
#define TID_MASK 0x7ffff
#define NO_PID (PID_MAX + 1)
#define THREAD_PID_OFFSET 100000

#define PS_CONTROLT 0x00000001  /* Has a controlling terminal */
#define PS_EXEC 0x00000002      /* Process called exec */
#define PS_INEXEC 0x00000004    /* Process is doing exec now */
#define PS_EXITING 0x00000008   /* Process is exiting */
#define PS_SUGID 0x00000010     /* Had set id privs since last exec */
#define PS_SUGIDEXEC 0x00000020 /* Last execve() was set[ug]id */
#define PS_PPWAIT 0x00000040    /* Parent waits for exec/exit */
#define PS_SYSTEM 0x00010000    /* System process (no signals) */
#define PS_EMBRYO 0x00020000    /* New process, not yet fledged */
#define PS_ZOMBIE 0x00040000    /* Dead and ready to be waited for */

#define P_INKTR 0x00000001  /* In a ktrace op, don't recurse */
#define P_SINTR 0x00000080  /* Sleep is interruptible */
#define P_SYSTEM 0x00000200 /* System thread */
#define P_WEXIT 0x00002000  /* Working on exiting */
#define P_THREAD 0x04000000 /* Only a thread, not a real process */
#define P_CPUPEG 0x40000000 /* Do not move to another cpu */

#define SIDL 1    /* Thread being created */
#define SRUN 2    /* Currently runnable */
#define SSLEEP 3  /* Sleeping on an address */
#define SSTOP 4   /* Debugging or suspension */
#define SDEAD 6   /* Thread is almost gone */
#define SONPROC 7 /* Thread is currently on a CPU */

#define PROCESS_KERNEL_STACK_SIZE (16 * 1024)
#define PROCESS_USER_STACK_SIZE (2 * 1024 * 1024)
#define USER_STACK_TOP 0x00007FFFFFFFF000ULL
#define USER_CODE_BASE 0x0000000000400000ULL

struct tusage {
	uint64_t tu_runtime; /* Real time in microseconds */
	uint64_t tu_uticks;  /* User mode ticks */
	uint64_t tu_sticks;  /* System mode ticks */
};

struct proc;
struct process {
	/* Reference counting */
	volatile unsigned int ps_refcnt; /* [a] Reference count */

	/* Identity */
	pid_t ps_pid;             /* [I] Process identifier */
	char ps_comm[_MAXCOMLEN]; /* Command name including NUL */

	/* Main thread */
	struct proc *ps_mainproc; /* Main thread in process */

	/* Thread list */
	TAILQ_HEAD(, proc) ps_threads; /* Threads in this process */
	unsigned int ps_threadcnt;     /* Number of threads */

	/* Process relationships */
	struct process *ps_pptr;          /* Pointer to parent process */
	LIST_ENTRY(process) ps_sibling;   /* List of sibling processes */
	LIST_HEAD(, process) ps_children; /* List of children */
	LIST_ENTRY(process) ps_list;      /* List of all processes */
	LIST_ENTRY(process) ps_hash;      /* Hash chain */

	/* Memory management */
	page_directory_t *ps_vmspace; /* Address space */
	uint64_t ps_strings;          /* User pointers to argv/env */
	uint64_t ps_brk;              /* Program break for heap */

	/* State and flags */
	unsigned int ps_flags; /* [a] PS_* flags */
	int ps_xexit;          /* Exit status for wait */
	int ps_xsig;           /* Stopping or killing signal */

	/* Accounting */
	struct tusage ps_tu;      /* Accumulated times */
	struct timespec ps_start; /* Process start time */

	/* Single-threading support */
	struct proc *ps_single; /* Thread for single-threading */
};

struct proc {
	/* Run queue linkage */
	TAILQ_ENTRY(proc) p_runq; /* [S] Current run/sleep queue */
	TAILQ_ENTRY(proc)
	p_list; /* List of all threads - TAILQ for easy iteration */
	LIST_ENTRY(proc) p_hash; /* TID hash chain */

	/* Process association */
	struct process *p_p;          /* [I] The process of this thread */
	TAILQ_ENTRY(proc) p_thr_link; /* Threads in process linkage */

	/* Identity */
	pid_t p_tid;             /* Thread identifier */
	char p_name[_MAXCOMLEN]; /* Thread name */

	/* State */
	int p_flag;       /* P_* flags */
	char p_stat;      /* [S] Process status (SIDL, SRUN, etc) */
	uint8_t p_runpri; /* [S] Runqueue priority */

	/* Memory - cached from process */
	page_directory_t *p_vmspace; /* [I] Copy of ps_vmspace */

	/* Stacks */
	void *p_kstack;        /* Kernel stack */
	uint64_t p_kstack_top; /* Top of kernel stack */
	uint64_t p_ustack_top; /* Top of user stack */

	/* Machine-dependent */
	struct mdproc p_md; /* Machine-dependent fields */

	/* Scheduling */
	const volatile void *p_wchan; /* [S] Sleep address */
	const char *p_wmesg;          /* [S] Reason for sleep */
	unsigned int p_cpticks;       /* Ticks of CPU time */
	uint64_t p_slptime;           /* Time since last blocked */

	/* Accounting */
	struct tusage p_tu; /* Accumulated times */

	/* CPU affinity */
	struct cpu_info *p_cpu; /* [S] CPU running on */
};

#define PIDHASH(pid) (&pidhashtbl[(pid) & pidhash])
#define TIDHASH(tid) (&tidhashtbl[(tid) & tidhash])

LIST_HEAD(pidhashhead, process);
LIST_HEAD(tidhashhead, proc);
LIST_HEAD(processlist, process);
TAILQ_HEAD(proclist, proc);

extern struct pidhashhead *pidhashtbl;
extern struct tidhashhead *tidhashtbl;
extern unsigned long pidhash;
extern unsigned long tidhash;

extern struct processlist allprocess;  /* List of all processes */
extern struct processlist zombprocess; /* List of zombie processes */
extern struct proclist allproc;        /* List of all threads */
extern struct proclist runqueue;       /* Ready to run threads */

extern struct process *initprocess; /* Init process */
extern struct proc *curproc;        /* Current thread */

void proc_init(void);
struct process *process_alloc(const char *name);
void process_free(struct process *ps);
struct proc *proc_alloc(struct process *ps, const char *name);
void proc_free(struct proc *p);

struct process *prfind(pid_t pid); /* Find process by PID */
struct proc *tfind(pid_t tid);     /* Find thread by TID */

void proc_enter_usermode(struct proc *p, uint64_t entry, uint64_t stack)
    __attribute__((noreturn));

static inline void
process_addref(struct process *ps)
{
	atomic_inc_int(&ps->ps_refcnt);
}

static inline void
process_release(struct process *ps)
{
	if (atomic_dec_int_nv(&ps->ps_refcnt) == 0) {
		process_free(ps);
	}
}

#define curprocess (curproc ? curproc->p_p : NULL)

static inline struct proc *
proc_get_current(void)
{
	return curproc;
}

static inline void
proc_set_current(struct proc *p)
{
	curproc = p;
}

#endif