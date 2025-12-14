#include <proc.h>
#include <sys/queue.h>
#include <sys/atomic.h>
#include <kmalloc.h>
#include <kfree.h>
#include <paging.h>
#include <pmm.h>
#include <gdt.h>
#include <segments.h>
#include <mmu.h>
#include <string.h>
#include <printf.h>

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

static pid_t
allocpid(void)
{
	pid_t pid;

	do {
		pid = nextpid++;
		if (nextpid > PID_MAX)
			nextpid = 1;
	} while (prfind(pid) != NULL);

	return pid;
}

static pid_t
alloctid(void)
{
	pid_t tid;

	do {
		tid = nexttid++;
		nexttid &= TID_MASK;
		if (nexttid == 0)
			nexttid = 1;
	} while (tfind(tid) != NULL);

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
    	printf_("Failed to allocate process hash tables\n");
    	pidhash = 0;
    	tidhash = 0;
    	if (pidhashtbl) kfree(pidhashtbl);
    	if (tidhashtbl) kfree(tidhashtbl);
    	pidhashtbl = NULL;
    	tidhashtbl = NULL;
    	return;
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

	/* Add to process list and hash table */
	LIST_INSERT_HEAD(&allprocess, ps, ps_list);
	LIST_INSERT_HEAD(PIDHASH(ps->ps_pid), ps, ps_hash);

	printf_("Allocated process %d (%s)\n", ps->ps_pid, ps->ps_comm);

	return ps;
}

void
process_free(struct process *ps)
{
	if (!ps)
		return;

	printf_("Freeing process %d (%s)\n", ps->ps_pid, ps->ps_comm);

	/* Remove from lists */
	LIST_REMOVE(ps, ps_list);
	LIST_REMOVE(ps, ps_hash);

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

	/* Add to process thread list */
	TAILQ_INSERT_TAIL(&ps->ps_threads, p, p_thr_link);
	atomic_inc_int(&ps->ps_threadcnt);

	/* Set as main thread if this is the first */
	if (!ps->ps_mainproc) {
		ps->ps_mainproc = p;
	}

	/* Add to global thread list and hash table */
	TAILQ_INSERT_TAIL(&allproc, p, p_list);
	LIST_INSERT_HEAD(TIDHASH(p->p_tid), p, p_hash);

	printf_("Allocated thread %d in process %d (%s)\n",
	        p->p_tid,
	        ps->ps_pid,
	        p->p_name);

	return p;
}

void
proc_free(struct proc *p)
{
	struct process *ps;

	if (!ps)
    return;

    if (ps->ps_threadcnt > 0) {
        printf_("WARNING: Freeing process %d with %d threads still alive\n",
                ps->ps_pid, ps->ps_threadcnt);
    }

	ps = p->p_p;

	printf_("Freeing thread %d (%s)\n", p->p_tid, p->p_name);

	/* Remove from process thread list */
	if (ps) {
		TAILQ_REMOVE(&ps->ps_threads, p, p_thr_link);
		atomic_dec_int(&ps->ps_threadcnt);

		if (ps->ps_mainproc == p) {
    	    /* Pick a new main thread if any remain */
   			ps->ps_mainproc = TAILQ_FIRST(&ps->ps_threads);
	}

	/* Remove from global lists */
	TAILQ_REMOVE(&allproc, p, p_list);
	LIST_REMOVE(p, p_hash);

	/* Free kernel stack */
	if (p->p_kstack) {
		kfree(p->p_kstack);
	}

	kfree(p);
	}
}


struct process *
prfind(pid_t pid)
{
	struct process *ps;
	struct pidhashhead *list;

	list = PIDHASH(pid);
	LIST_FOREACH(ps, list, ps_hash)
	{
		if (ps->ps_pid == pid)
			return ps;
	}

	return NULL;
}

struct proc *
tfind(pid_t tid)
{
	struct proc *p;
	struct tidhashhead *list;

	list = TIDHASH(tid);
	LIST_FOREACH(p, list, p_hash)
	{
		if (p->p_tid == tid)
			return p;
	}

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

	printf_("Process: %d (%s), Thread: %d (%s)\n",
	        ps->ps_pid,
	        ps->ps_comm,
	        p->p_tid,
	        p->p_name);
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
	} while (atomic_cas_uint(&ps->ps_flags, old_flags, new_flags) !=
	         old_flags);

	/* Set TSS kernel stack */
	tss_set_rsp0(p->p_kstack_top);

	/* Prepare for iretq */
	uint64_t user_cs = GDT_SELECTOR_USER_CODE;
	uint64_t user_ss = GDT_SELECTOR_USER_DATA;
	uint64_t rflags = 0x202; /* IF=1, Reserved bit */

	/* Align stack */
	if (stack_top & 0xF) {
		stack_top &= ~0xFULL;
	}

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
	             : "r"(entry_point),
	               "r"(stack_top),
	               "r"(user_ss),
	               "r"(user_cs),
	               "r"(rflags),
	               "r"(user_cr3)
	             : "memory");

	__builtin_unreachable();
}

void
proc_fork_child_entry(void *arg)
{
	struct proc *p = (struct proc *)arg;
	struct trapframe *tf = (struct trapframe *)p->p_md.md_regs;

	if (!p || !tf) {
		printf_("proc_fork_child_entry: invalid arguments\n");
		while (1)
			asm("hlt");
	}

	struct process *ps = p->p_p;
	printf_(
	    "Child process %d (thread %d) starting\n", ps->ps_pid, p->p_tid);

	proc_set_current(p);
	p->p_stat = SONPROC;

	uint64_t user_cr3 = ps->ps_vmspace->phys_addr;

	tss_set_rsp0(p->p_kstack_top);

	/* Return to userspace using the saved trapframe */
	asm volatile("cli\n"

	             /* Load registers from trapframe */
	             "movq %0, %%rsp\n" /* Point RSP to trapframe */

	             /* Load general purpose registers */
	             "movq 0(%%rsp), %%r15\n"
	             "movq 8(%%rsp), %%r14\n"
	             "movq 16(%%rsp), %%r13\n"
	             "movq 24(%%rsp), %%r12\n"
	             "movq 32(%%rsp), %%r11\n"
	             "movq 40(%%rsp), %%r10\n"
	             "movq 48(%%rsp), %%r9\n"
	             "movq 56(%%rsp), %%r8\n"
	             "movq 64(%%rsp), %%rbp\n"
	             "movq 72(%%rsp), %%rdi\n"
	             "movq 80(%%rsp), %%rsi\n"
	             "movq 88(%%rsp), %%rdx\n"
	             "movq 96(%%rsp), %%rcx\n"
	             "movq 104(%%rsp), %%rbx\n"
	             "movq 112(%%rsp), %%rax\n" /* RAX=0 for child */

	             /* Skip trapno and err (already processed) */
	             /* Load user CR3 */
	             "movq %1, %%cr3\n"

	             /* Set up data segments for user mode */
	             "movw $0x23, %%ax\n" /* User data selector */
	             "movw %%ax, %%ds\n"
	             "movw %%ax, %%es\n"
	             "movw %%ax, %%fs\n"
	             "movw %%ax, %%gs\n"

	             /* Point to iret frame */
	             "addq $136, %%rsp\n" /* Skip to tf_rip */

	             /* Stack now has: RIP, CS, RFLAGS, RSP, SS */
	             "iretq\n"
	             :
	             : "r"(tf), "r"(user_cr3)
	             : "memory");

	__builtin_unreachable();
}