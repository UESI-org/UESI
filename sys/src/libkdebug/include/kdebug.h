#ifndef KDEBUG_H
#define KDEBUG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "isr.h"

#define KDEBUG_LEVEL_NONE    0
#define KDEBUG_LEVEL_ERROR   1
#define KDEBUG_LEVEL_WARN    2
#define KDEBUG_LEVEL_INFO    3
#define KDEBUG_LEVEL_DEBUG   4
#define KDEBUG_LEVEL_TRACE   5

#define KDEBUG_MAX_STACK_DEPTH 32

typedef struct stack_frame {
    struct stack_frame *rbp;
    uint64_t rip;
} stack_frame_t;

typedef struct {
    uint64_t address;
    const char *name;
    uint64_t offset;
} kdebug_symbol_t;

typedef struct {
    uint64_t address;
    uint8_t original_byte;
    bool enabled;
    void (*callback)(registers_t *regs);
} kdebug_breakpoint_t;

void kdebug_init(void);

void kdebug_set_level(int level);

int kdebug_get_level(void);

void kdebug_enable_serial(bool enable);

void kdebug_panic(const char *message, registers_t *regs) __attribute__((noreturn));

void kdebug_dump_registers(registers_t *regs);

void kdebug_backtrace(void);

void kdebug_backtrace_from(uint64_t rbp, uint64_t rip);

void kdebug_dump_memory(uint64_t address, size_t length);

void kdebug_hexdump(const void *data, size_t length);

bool kdebug_set_breakpoint(uint64_t address, void (*callback)(registers_t *));
bool kdebug_clear_breakpoint(uint64_t address);
void kdebug_list_breakpoints(void);

void kdebug_breakpoint_handler(registers_t *regs);

void kdebug_register_symbol(uint64_t address, const char *name);

kdebug_symbol_t kdebug_lookup_symbol(uint64_t address);

#define KASSERT(expr) \
    do { \
        if (!(expr)) { \
            kdebug_assert_failed(#expr, __FILE__, __LINE__, __func__); \
        } \
    } while(0)

#define KASSERT_MSG(expr, msg) \
    do { \
        if (!(expr)) { \
            kdebug_assert_failed_msg(#expr, msg, __FILE__, __LINE__, __func__); \
        } \
    } while(0)

#define KDEBUG_LOG(level, fmt, ...) \
    kdebug_log(level, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define KDEBUG_ERROR(fmt, ...) \
    KDEBUG_LOG(KDEBUG_LEVEL_ERROR, fmt, ##__VA_ARGS__)

#define KDEBUG_WARN(fmt, ...) \
    KDEBUG_LOG(KDEBUG_LEVEL_WARN, fmt, ##__VA_ARGS__)

#define KDEBUG_INFO(fmt, ...) \
    KDEBUG_LOG(KDEBUG_LEVEL_INFO, fmt, ##__VA_ARGS__)

#define KDEBUG_DEBUG(fmt, ...) \
    KDEBUG_LOG(KDEBUG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)

#define KDEBUG_TRACE(fmt, ...) \
    KDEBUG_LOG(KDEBUG_LEVEL_TRACE, fmt, ##__VA_ARGS__)

void kdebug_assert_failed(const char *expr, const char *file, 
                         int line, const char *func) __attribute__((noreturn));

void kdebug_assert_failed_msg(const char *expr, const char *msg,
                              const char *file, int line, 
                              const char *func) __attribute__((noreturn));

void kdebug_log(int level, const char *file, int line, 
                const char *func, const char *fmt, ...);

void kdebug_dump_control_registers(void);

void kdebug_dump_segments(void);

bool kdebug_is_valid_address(uint64_t address);

void kdebug_dump_page_info(uint64_t virtual_address);

#endif