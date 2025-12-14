#ifndef _MACHINE_INTR_H_
#define _MACHINE_INTR_H_

#include <stdint.h>
#include <stdbool.h>

static inline uint64_t
read_rflags(void)
{
	uint64_t flags;
	__asm__ volatile("pushfq; pop %0" : "=r"(flags)::"memory");
	return flags;
}

static inline void
write_rflags(uint64_t flags)
{
	__asm__ volatile("push %0; popfq" ::"r"(flags) : "memory", "cc");
}

static inline void
cli(void)
{
	__asm__ volatile("cli" ::: "memory");
}

static inline void
sti(void)
{
	__asm__ volatile("sti" ::: "memory");
}

static inline bool
interrupts_enabled(void)
{
	return (read_rflags() & 0x200) != 0; /* IF flag is bit 9 */
}

static inline uint64_t
intr_disable(void)
{
	uint64_t flags = read_rflags();
	cli();
	return flags;
}

static inline void
intr_restore(uint64_t flags)
{
	write_rflags(flags);
}

#endif