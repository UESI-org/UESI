#ifndef PANIC_H
#define PANIC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <isr.h>

typedef enum {
	PANIC_GENERAL = 0,
	PANIC_HARDWARE_FAULT,
	PANIC_MEMORY_CORRUPTION,
	PANIC_ASSERTION_FAILED,
	PANIC_STACK_OVERFLOW,
	PANIC_DIVIDE_BY_ZERO,
	PANIC_PAGE_FAULT,
	PANIC_DOUBLE_FAULT,
	PANIC_INVALID_OPCODE,
	PANIC_GPF,
	PANIC_MACHINE_CHECK,
	PANIC_OUT_OF_MEMORY,
	PANIC_DEADLOCK,
	PANIC_UNKNOWN
} panic_code_t;

__attribute__((noreturn)) void panic(const char *message);

__attribute__((noreturn)) void panic_fmt(const char *fmt, ...);

__attribute__((noreturn)) void panic_registers(const char *message,
                                               registers_t *regs,
                                               panic_code_t code);

__attribute__((noreturn)) void panic_assert(const char *expr,
                                            const char *file,
                                            int line,
                                            const char *func);

bool panic_in_progress(void);

const char *panic_code_name(panic_code_t code);

#ifndef NDEBUG
#define ASSERT(expr)                                                           \
	do {                                                                   \
		if (!(expr)) {                                                 \
			panic_assert(#expr, __FILE__, __LINE__, __func__);     \
		}                                                              \
	} while (0)
#else
#define ASSERT(expr) ((void)0)
#endif

#define PANIC(msg) panic_fmt("PANIC at %s:%d: %s", __FILE__, __LINE__, (msg))

#define STATIC_ASSERT(expr, msg) _Static_assert(expr, msg)

#endif