#include <stdlib.h>
#include <sys/cdefs.h>

#define MAX_ATEXIT 32

static void (*atexit_funcs[MAX_ATEXIT])(void);
static int atexit_count = 0;

int
atexit(void (*function)(void))
{
	if (function == NULL || atexit_count >= MAX_ATEXIT) {
		return -1;
	}
	atexit_funcs[atexit_count++] = function;
	return 0;
}

/* _exit() - Direct syscall without atexit handlers (POSIX) */
static __dead void
_exit(int status)
{
#ifdef _KERNEL
	extern void sys_exit(int status) __attribute__((noreturn));
	sys_exit(status);
#else
	/* Direct syscall: SYSCALL_EXIT = 1 */
	__asm__ volatile("int $0x80"
	                 :
	                 : "a"((uint64_t)1), "D"((uint64_t)status)
	                 : "memory", "rcx", "r11");
	__builtin_unreachable();
#endif
}

__dead void
_Exit(int status)
{
	_exit(status);
	__builtin_unreachable();
}

__dead void
exit(int status)
{
	/* Call atexit handlers in reverse order */
	for (int i = atexit_count - 1; i >= 0; i--) {
		if (atexit_funcs[i] != NULL) {
			atexit_funcs[i]();
		}
	}

	_exit(status);
	__builtin_unreachable();
}
