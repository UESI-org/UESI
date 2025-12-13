#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

#define PIT_FREQUENCY 1193182
#define PIT_CHANNEL0 0x40
#define PIT_CHANNEL1 0x41
#define PIT_CHANNEL2 0x42
#define PIT_COMMAND 0x43

#define PIT_CHANNEL_0 0x00
#define PIT_CHANNEL_1 0x40
#define PIT_CHANNEL_2 0x80
#define PIT_ACCESS_LATCH 0x00
#define PIT_ACCESS_LO 0x10
#define PIT_ACCESS_HI 0x20
#define PIT_ACCESS_LOHI 0x30
#define PIT_MODE_0 0x00
#define PIT_MODE_1 0x02
#define PIT_MODE_2 0x04
#define PIT_MODE_3 0x06
#define PIT_MODE_4 0x08
#define PIT_MODE_5 0x0A
#define PIT_BINARY 0x00
#define PIT_BCD 0x01

#define TIMER_FREQ_HZ 100
#define TIMER_IRQ 0

typedef void (*timer_callback_t)(void *data);

typedef struct timer_callback_node {
	timer_callback_t callback;
	void *data;
	uint64_t fire_tick;
	uint64_t interval;
	uint8_t active;
	struct timer_callback_node *next;
} timer_callback_node_t;

typedef struct {
	uint64_t total_ticks;
	uint64_t total_interrupts;
	uint32_t frequency_hz;
	uint32_t active_callbacks;
} timer_stats_t;

void timer_init(uint32_t frequency_hz);
uint64_t timer_get_ticks(void);
uint64_t timer_get_uptime_ms(void);
uint64_t timer_get_uptime_sec(void);

void timer_delay_ms(uint32_t ms);
void timer_delay_ticks(uint64_t ticks);

timer_callback_node_t *timer_register_oneshot(timer_callback_t callback,
                                              void *data,
                                              uint32_t delay_ms);
timer_callback_node_t *timer_register_periodic(timer_callback_t callback,
                                               void *data,
                                               uint32_t interval_ms);
void timer_cancel_callback(timer_callback_node_t *node);

void timer_set_frequency(uint32_t frequency_hz);
uint32_t timer_get_frequency(void);
void timer_get_stats(timer_stats_t *stats);

void timer_sleep_ms(uint32_t ms);

#endif