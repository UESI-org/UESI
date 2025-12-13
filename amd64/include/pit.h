#ifndef PIT_H
#define PIT_H

#include <stdint.h>

#define PIT_CHANNEL0 0x40
#define PIT_CHANNEL1 0x41
#define PIT_CHANNEL2 0x42
#define PIT_COMMAND 0x43

#define PIT_BASE_FREQ 1193182
#define PIT_MAX_FREQ 1193182
#define PIT_MIN_FREQ 18

#define PIT_IRQ 0

#define PIT_SELECT_CHANNEL0 0x00 // 00b - Channel 0
#define PIT_SELECT_CHANNEL1 0x40 // 01b - Channel 1
#define PIT_SELECT_CHANNEL2 0x80 // 10b - Channel 2
#define PIT_READBACK 0xC0        // 11b - Read-back command

#define PIT_ACCESS_LATCH 0x00  // 00b - Latch count value
#define PIT_ACCESS_LOBYTE 0x10 // 01b - Access low byte only
#define PIT_ACCESS_HIBYTE 0x20 // 10b - Access high byte only
#define PIT_ACCESS_LOHI 0x30   // 11b - Access low byte, then high byte

#define PIT_MODE_0 0x00 // 000b - Interrupt on terminal count
#define PIT_MODE_1 0x02 // 001b - Hardware re-triggerable one-shot
#define PIT_MODE_2 0x04 // 010b - Rate generator
#define PIT_MODE_3 0x06 // 011b - Square wave generator
#define PIT_MODE_4 0x08 // 100b - Software triggered strobe
#define PIT_MODE_5 0x0A // 101b - Hardware triggered strobe

#define PIT_BINARY 0x00
#define PIT_BCD 0x01

#define PIT_CMD_COUNTER0_RATE                                                  \
	(PIT_SELECT_CHANNEL0 | PIT_ACCESS_LOHI | PIT_MODE_2 | PIT_BINARY)
#define PIT_CMD_COUNTER0_ONESHOT                                               \
	(PIT_SELECT_CHANNEL0 | PIT_ACCESS_LOHI | PIT_MODE_0 | PIT_BINARY)
#define PIT_CMD_COUNTER0_SQUARE                                                \
	(PIT_SELECT_CHANNEL0 | PIT_ACCESS_LOHI | PIT_MODE_3 | PIT_BINARY)

#define PIT_FREQ_100HZ 100   // 10ms tick (common for schedulers)
#define PIT_FREQ_1000HZ 1000 // 1ms tick (high precision)
#define PIT_FREQ_18HZ 18     // ~18.2065 Hz (original BIOS rate)

typedef enum {
	PIT_MODE_TERMINAL_COUNT = 0,
	PIT_MODE_ONESHOT = 1,
	PIT_MODE_RATE_GEN = 2,
	PIT_MODE_SQUARE_WAVE = 3,
	PIT_MODE_HW_STROBE = 5
} pit_mode_t;

typedef enum {
	PIT_CHANNEL_0 = 0,
	PIT_CHANNEL_1 = 1,
	PIT_CHANNEL_2 = 2
} pit_channel_t;

uint32_t pit_init(uint32_t frequency);

uint32_t pit_set_frequency(uint32_t frequency);

void pit_set_count(pit_channel_t channel, uint16_t divisor, pit_mode_t mode);

uint16_t pit_read_count(pit_channel_t channel);

void pit_oneshot(uint32_t microseconds);

void pit_start(void);

void pit_stop(void);

uint16_t pit_calculate_divisor(uint32_t frequency);

uint32_t pit_get_frequency(void);

uint64_t pit_get_ticks(void);

void pit_handler(void);

void pit_delay(uint32_t milliseconds);

#endif