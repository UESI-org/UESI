#include <proc.h>
#include <sys/queue.h>
#include <sys/atomic.h>
#include <sys/spinlock.h>
#include <kmalloc.h>
#include <kfree.h>
#include <paging.h>
#include <pmm.h>
#include <gdt.h>
#include <segments.h>
#include <mmu.h>
#include <string.h>
#include <printf.h>
#include <tapframe.h>

struct processlist allprocess = LIST_HEAD_INITIALIZER(allprocess);
struct processlist zombprocess = LIST_HEAD_INITIALIZER(zombprocess);
struct proclist allproc = TAILQ_HEAD_INITIALIZER(allproc);
struct proclist runqueue = TAILQ_HEAD_INITIALIZER(runqueue);

struct pidhashhead *pidhashtbl = NULL;
struct tidhashhead *tidhashtbl = NULL;
unsigned long pidhash = 0;
unsigned long tidhash = 0;

struct process *initprocess = NULL;
struct proc *curproc = NULL;

static pid_t nextpid = 1;
static pid_t nexttid = 1;

static spinlock_t pid_alloc_lock = SPINLOCK_INITIALIZER("pidalloc");
static spinlock_t tid_alloc_lock = SPINLOCK_INITIALIZER("tidalloc");
static spinlock_t allprocess_lock = SPINLOCK_INITIALIZER("allprocess");
static spinlock_t allproc_lock = SPINLOCK_INITIALIZER("allproc");
static spinlock_t pidhash_lock = SPINLOCK_INITIALIZER("pidhash");
static spinlock_t tidhash_lock = SPINLOCK_INITIALIZER("tidhash");

static pid_t
allocpid(void)
{
	pid_t pid;

	spinlock_acquire(&pid_alloc_lock);
	do {
		pid = nextpid++;
		if (nextpid > PID_MAX)
			nextpid = 1;
	} while (prfind(pid) != NULL);
	spinlock_release(&pid_alloc_lock);

	return pid;
}

static pid_t
alloctid(void)
{
	pid_t tid;

	spinlock_acquire(&tid_alloc_lock);
	do {
		tid = nexttid++;
		nexttid &= TID_MASK;
		if (nexttid == 0)
			nexttid = 1;
	} while (tfind(tid) != NULL);
	spinlock_release(&tid_alloc_lock);

	return tid;
}

void
proc_init(void)
{
	/* Initialize hash tables */
	pidhash = 127;
	tidhash = 127;

	pidhashtbl = kmalloc((pidhash + 1) * sizeof(struct pidhashhead));
	tidhashtbl = kmalloc((tidhash + 1) * sizeof(struct tidhashhead));

	if (!pidhashtbl || !tidhashtbl) {
		printf_("FATAL: Failed to allocate process hash tables\n");
		if (pidhashtbl)
			kfree(pidhashtbl);
		if (tidhashtbl)
			kfree(tidhashtbl);
		/* This is fatal - cannot continue without hash tables */
		while (1)
			asm("hlt");
	}

	for (unsigned long i = 0; i <= pidhash; i++)
		LIST_INIT(&pidhashtbl[i]);

	for (unsigned long i = 0; i <= tidhash; i++)
		LIST_INIT(&tidhashtbl[i]);

	printf_("Process subsystem initialized\n");
}

struct process *
process_alloc(const char *name)
{
	struct process *ps;

	ps = kmalloc(sizeof(struct process));
	if (!ps) {
		printf_("Failed to allocate process structure\n");
		return NULL;
	}

	memset(ps, 0, sizeof(struct process));

	/* Initialize the process lock */
	spinlock_init(&ps->ps_lock, "process");

	/* Initialize reference count */
	ps->ps_refcnt = 1;

	/* Allocate PID */
	ps->ps_pid = allocpid();

	/* Set name */
	strncpy(ps->ps_comm, name, _MAXCOMLEN - 1);
	ps->ps_comm[_MAXCOMLEN - 1] = '\0';

	/* Initialize thread list */
	TAILQ_INIT(&ps->ps_threads);
	ps->ps_threadcnt = 0;

	/* Initialize children list */
	LIST_INIT(&ps->ps_children);

	/* Create address space */
	ps->ps_vmspace = paging_create_user_address_space();
	if (!ps->ps_vmspace) {
		printf_("Failed to create address space\n");
		kfree(ps);
		return NULL;
	}

	/* Set initial flags */
	ps->ps_flags = PS_EMBRYO;

	/* Add to process list and hash table - protected by global locks */
	spinlock_acquire(&allprocess_lock);
	LIST_INSERT_HEAD(&allprocess, ps, ps_list);
	spinlock_release(&allprocess_lock);

	spinlock_acquire(&pidhash_lock);
	LIST_INSERT_HEAD(PIDHASH(ps->ps_pid), ps, ps_hash);
	spinlock_release(&pidhash_lock);

	printf_("Allocated process %d (%s)\n", ps->ps_pid, ps->ps_comm);

	return ps;
}

void
process_free(struct process *ps)
{
	if (!ps)
		return;

	printf_("Freeing process %d (%s)\n", ps->ps_pid, ps->ps_comm);

	/* Verify no threads remain */
	spinlock_acquire(&ps->ps_lock);
	if (ps->ps_threadcnt > 0) {
		spinlock_release(&ps->ps_lock);
		printf_("ERROR: Attempt to free process %d with %d threads still alive\n",
		        ps->ps_pid, ps->ps_threadcnt);
		return;
	}
	spinlock_release(&ps->ps_lock);

	/* Remove from global lists */
	spinlock_acquire(&allprocess_lock);
	LIST_REMOVE(ps, ps_list);
	spinlock_release(&allprocess_lock);

	spinlock_acquire(&pidhash_lock);
	LIST_REMOVE(ps, ps_hash);
	spinlock_release(&pidhash_lock);

	/* Free address space */
	if (ps->ps_vmspace) {
		paging_free_user_pages(ps->ps_vmspace);
		mmu_destroy_address_space(ps->ps_vmspace);
	}

	kfree(ps);
}

struct proc *
proc_alloc(struct process *ps, const char *name)
{
	struct proc *p;

	if (!ps) {
		printf_("Cannot allocate thread: null process\n");
		return NULL;
	}

	p = kmalloc(sizeof(struct proc));
	if (!p) {
		printf_("Failed to allocate thread structure\n");
		return NULL;
	}

	memset(p, 0, sizeof(struct proc));

	/* Link to process */
	p->p_p = ps;
	p->p_vmspace = ps->ps_vmspace;

	/* Allocate TID */
	p->p_tid = alloctid();

	/* Set name */
	if (name) {
		strncpy(p->p_name, name, _MAXCOMLEN - 1);
		p->p_name[_MAXCOMLEN - 1] = '\0';
	} else {
		strncpy(p->p_name, ps->ps_comm, _MAXCOMLEN - 1);
	}

	/* Allocate kernel stack */
	p->p_kstack = kmalloc(PROCESS_KERNEL_STACK_SIZE);
	if (!p->p_kstack) {
		printf_("Failed to allocate kernel stack\n");
		kfree(p);
		return NULL;
	}

	memset(p->p_kstack, 0, PROCESS_KERNEL_STACK_SIZE);
	p->p_kstack_top = (uint64_t)p->p_kstack + PROCESS_KERNEL_STACK_SIZE;

	/* Set user stack top */
	p->p_ustack_top = USER_STACK_TOP;

	/* Set initial state */
	p->p_stat = SIDL;
	p->p_flag = 0;

	/* Add to process thread list - protected by process lock */
	spinlock_acquire(&ps->ps_lock);
	TAILQ_INSERT_TAIL(&ps->ps_threads, p, p_thr_link);
	atomic_inc_int(&ps->ps_threadcnt);

	/* Set as main thread if this is the first */
	if (!ps->ps_mainproc) {
		ps->ps_mainproc = p;
	}
	spinlock_release(&ps->ps_lock);

	/* Add to global thread list and hash table */
	spinlock_acquire(&allproc_lock);
	TAILQ_INSERT_TAIL(&allproc, p, p_list);
	spinlock_release(&allproc_lock);

	spinlock_acquire(&tidhash_lock);
	LIST_INSERT_HEAD(TIDHASH(p->p_tid), p, p_hash);
	spinlock_release(&tidhash_lock);

	printf_("Allocated thread %d in process %d (%s)\n",
	        p->p_tid, ps->ps_pid, p->p_name);

	return p;
}

void
proc_free(struct proc *p)
{
	struct process *ps;

	if (!p)
		return;

	ps = p->p_p;
	if (!ps)
		return;

	printf_("Freeing thread %d (%s)\n", p->p_tid, p->p_name);

	/* Remove from process thread list */
	spinlock_acquire(&ps->ps_lock);
	TAILQ_REMOVE(&ps->ps_threads, p, p_thr_link);
	atomic_dec_int(&ps->ps_threadcnt);

	if (ps->ps_mainproc == p) {
		/* Pick a new main thread if any remain */
		ps->ps_mainproc = TAILQ_FIRST(&ps->ps_threads);
	}

	/* Check if this was the last thread */
	unsigned int threadcnt = ps->ps_threadcnt;
	spinlock_release(&ps->ps_lock);

	if (threadcnt == 0) {
		/* Last thread - mark process as zombie */
		atomic_cas_uint(&ps->ps_flags, 
		                ps->ps_flags, 
		                ps->ps_flags | PS_ZOMBIE);
		
		/* Move to zombie list */
		spinlock_acquire(&allprocess_lock);
		LIST_REMOVE(ps, ps_list);
		LIST_INSERT_HEAD(&zombprocess, ps, ps_list);
		spinlock_release(&allprocess_lock);
		
		printf_("Process %d is now a zombie (last thread exited)\n", ps->ps_pid);
	}

	/* Remove from global thread list and hash */
	spinlock_acquire(&allproc_lock);
	TAILQ_REMOVE(&allproc, p, p_list);
	spinlock_release(&allproc_lock);

	spinlock_acquire(&tidhash_lock);
	LIST_REMOVE(p, p_hash);
	spinlock_release(&tidhash_lock);

	/* Free kernel stack */
	if (p->p_kstack) {
		kfree(p->p_kstack);
	}

	kfree(p);
}

struct process *
prfind(pid_t pid)
{
	struct process *ps;
	struct pidhashhead *list;

	spinlock_acquire(&pidhash_lock);
	list = PIDHASH(pid);
	LIST_FOREACH(ps, list, ps_hash) {
		if (ps->ps_pid == pid) {
			spinlock_release(&pidhash_lock);
			return ps;
		}
	}
	spinlock_release(&pidhash_lock);

	return NULL;
}

struct proc *
tfind(pid_t tid)
{
	struct proc *p;
	struct tidhashhead *list;

	spinlock_acquire(&tidhash_lock);
	list = TIDHASH(tid);
	LIST_FOREACH(p, list, p_hash) {
		if (p->p_tid == tid) {
			spinlock_release(&tidhash_lock);
			return p;
		}
	}
	spinlock_release(&tidhash_lock);

	return NULL;
}


void
proc_enter_usermode(struct proc *p, uint64_t entry_point, uint64_t stack_top)
{
	struct process *ps;

	if (!p) {
		printf_("Cannot enter usermode: null thread\n");
		while (1)
			asm("hlt");
	}

	ps = p->p_p;

	/* Validate entry point and stack */
	if (entry_point >= 0x00007FFFFFFFFFFF) {
		printf_("Invalid entry point for usermode: 0x%016lx\n", entry_point);
		while (1)
			asm("hlt");
	}

	if (stack_top & 0xF) {
		printf_("WARNING: Unaligned stack 0x%lx, aligning\n", stack_top);
		stack_top &= ~0xFULL;
	}

	printf_("Process: %d (%s), Thread: %d (%s)\n",
	        ps->ps_pid, ps->ps_comm, p->p_tid, p->p_name);
	printf_("Entry: 0x%016lx, Stack: 0x%016lx\n", entry_point, stack_top);

	/* Set as current thread */
	proc_set_current(p);

	/* Update state */
	p->p_stat = SRUN;

	/* Clear EMBRYO flag and set EXEC flag atomically */
	unsigned int old_flags, new_flags;
	do {
		old_flags = ps->ps_flags;
		new_flags = (old_flags & ~PS_EMBRYO) | PS_EXEC;
	} while (atomic_cas_uint(&ps->ps_flags, old_flags, new_flags) != old_flags);

	/* Set TSS kernel stack */
	tss_set_rsp0(p->p_kstack_top);

	/* Prepare for iretq */
	uint64_t user_cs = GDT_SELECTOR_USER_CODE;
	uint64_t user_ss = GDT_SELECTOR_USER_DATA;
	uint64_t rflags = 0x202; /* IF=1, Reserved bit */

	/* Get physical address of user page directory */
	uint64_t user_cr3 = ps->ps_vmspace->phys_addr;

	/* Switch to user mode using iretq */
	asm volatile("cli\n"

	             /* Push iretq frame */
	             "pushq %2\n" /* SS */
	             "pushq %1\n" /* RSP */
	             "pushq %4\n" /* RFLAGS */
	             "pushq %3\n" /* CS */
	             "pushq %0\n" /* RIP */

	             /* Load user CR3 */
	             "movq %5, %%cr3\n"

	             /* Set data segments */
	             "movw %w2, %%ds\n"
	             "movw %w2, %%es\n"
	             "movw %w2, %%fs\n"
	             "movw %w2, %%gs\n"

	             /* Clear all registers */
	             "xorq %%rax, %%rax\n"
	             "xorq %%rbx, %%rbx\n"
	             "xorq %%rcx, %%rcx\n"
	             "xorq %%rdx, %%rdx\n"
	             "xorq %%rsi, %%rsi\n"
	             "xorq %%rdi, %%rdi\n"
	             "xorq %%rbp, %%rbp\n"
	             "xorq %%r8, %%r8\n"
	             "xorq %%r9, %%r9\n"
	             "xorq %%r10, %%r10\n"
	             "xorq %%r11, %%r11\n"
	             "xorq %%r12, %%r12\n"
	             "xorq %%r13, %%r13\n"
	             "xorq %%r14, %%r14\n"
	             "xorq %%r15, %%r15\n"

	             /* Jump to userspace */
	             "iretq\n"
	             :
	             : "r"(entry_point), "r"(stack_top), "r"(user_ss),
	               "r"(user_cs), "r"(rflags), "r"(user_cr3)
	             : "memory");

	__builtin_unreachable();
}

void
proc_fork_child_entry(void *arg)
{
	struct proc *p = (struct proc *)arg;
	
	if (!p) {
		printf_("proc_fork_child_entry: NULL proc!\n");
		while (1) asm("hlt");
	}
	
	struct trapframe *tf = (struct trapframe *)p->p_md.md_regs;
	if (!tf) {
		printf_("proc_fork_child_entry: NULL trapframe!\n");
		while (1) asm("hlt");
	}

	struct process *ps = p->p_p;
	if (!ps) {
		printf_("proc_fork_child_entry: NULL process!\n");
		while (1) asm("hlt");
	}

	printf_("Child process %d (thread %d) starting at RIP=0x%lx RSP=0x%lx\n",
	        ps->ps_pid, p->p_tid, tf->tf_rip, tf->tf_rsp);

	proc_set_current(p);
	p->p_stat = SONPROC;

	uint64_t user_cr3 = ps->ps_vmspace->phys_addr;
	/* Cast to uint64_t to force 64-bit registers for pushq */
	uint64_t user_data_sel = GDT_SELECTOR_USER_DATA;
	uint64_t user_code_sel = GDT_SELECTOR_USER_CODE;

	extern void tss_set_rsp0(uint64_t rsp);
	tss_set_rsp0(p->p_kstack_top);

	/* Return to userspace using the trapframe */
	asm volatile(
	    "cli\n"
	
	    /* Load user CR3 first */
	    "movq %1, %%cr3\n"
	
	    /* Load general purpose registers from trapframe */
	    "movq 112(%%rdi), %%rax\n"    /* tf_rax (child returns 0) */
	    "movq 104(%%rdi), %%rbx\n"    /* tf_rbx */
	    "movq 96(%%rdi), %%rcx\n"     /* tf_rcx */
	    "movq 88(%%rdi), %%rdx\n"     /* tf_rdx */
	    "movq 80(%%rdi), %%rsi\n"     /* tf_rsi */
	    "movq 64(%%rdi), %%rbp\n"     /* tf_rbp */
	    "movq 56(%%rdi), %%r8\n"      /* tf_r8 */
	    "movq 48(%%rdi), %%r9\n"      /* tf_r9 */
	    "movq 40(%%rdi), %%r10\n"     /* tf_r10 */
	    "movq 32(%%rdi), %%r11\n"     /* tf_r11 */
	    "movq 24(%%rdi), %%r12\n"     /* tf_r12 */
	    "movq 16(%%rdi), %%r13\n"     /* tf_r13 */
	    "movq 8(%%rdi), %%r14\n"      /* tf_r14 */
	    "movq 0(%%rdi), %%r15\n"      /* tf_r15 */
	
	    /* Set up data segments for user mode */
	    "movw %w2, %%cx\n"            /* User data selector */
	    "movw %%cx, %%ds\n"
	    "movw %%cx, %%es\n"
	    "movw %%cx, %%fs\n"
	    "movw %%cx, %%gs\n"
	
	    /* Build iret frame on current stack */
	    "pushq %2\n"                  /* SS (user data) */
	    "pushq 160(%%rdi)\n"          /* tf_rsp (user stack) */
	    "pushq 152(%%rdi)\n"          /* tf_rflags */
	    "pushq %3\n"                  /* CS (user code) */
	    "pushq 136(%%rdi)\n"          /* tf_rip (user return address) */
	
	    /* Load last register (rdi) */
	    "movq 72(%%rdi), %%rdi\n"     /* tf_rdi */
	
	    /* Jump to userspace */
	    "iretq\n"
	
	    : /* no outputs */
	    : "D"(tf), "r"(user_cr3), "r"(user_data_sel), "r"(user_code_sel)
	    : "memory");

	__builtin_unreachable();
}