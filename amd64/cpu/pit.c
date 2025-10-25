#include "pit.h"
#include "io.h"
#include "pic.h"

static volatile uint64_t pit_ticks = 0;
static uint32_t pit_frequency = 0;
static uint16_t pit_divisor = 0;

uint32_t pit_init(uint32_t frequency) {
    pit_ticks = 0;
    
    uint32_t actual_freq = pit_set_frequency(frequency);
    
    pic_clear_mask(PIT_IRQ);
    
    return actual_freq;
}

uint32_t pit_set_frequency(uint32_t frequency) {
    if (frequency < PIT_MIN_FREQ) {
        frequency = PIT_MIN_FREQ;
    } else if (frequency > PIT_MAX_FREQ) {
        frequency = PIT_MAX_FREQ;
    }
    
    pit_divisor = pit_calculate_divisor(frequency);
    
    pit_frequency = PIT_BASE_FREQ / pit_divisor;
    
    // Send command byte: Channel 0, Access mode lo/hi byte, Mode 2 (rate generator), Binary
    outb(PIT_COMMAND, PIT_CMD_COUNTER0_RATE);
    
    outb(PIT_CHANNEL0, (uint8_t)(pit_divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((pit_divisor >> 8) & 0xFF));
    
    return pit_frequency;
}

void pit_set_count(pit_channel_t channel, uint16_t divisor, pit_mode_t mode) {
    uint8_t command;
    uint16_t port;
    
    if (channel > PIT_CHANNEL_2) {
        return;
    }
    
    command = ((channel & 0x3) << 6) |
              PIT_ACCESS_LOHI |
              ((mode & 0x7) << 1) |
              PIT_BINARY;
    
    port = PIT_CHANNEL0 + channel;
    
    outb(PIT_COMMAND, command);
    
    outb(port, (uint8_t)(divisor & 0xFF));
    outb(port, (uint8_t)((divisor >> 8) & 0xFF));
}

uint16_t pit_read_count(pit_channel_t channel) {
    uint8_t low, high;
    uint16_t port;
    uint8_t command;
    
    if (channel > PIT_CHANNEL_2) {
        return 0;
    }
    
    port = PIT_CHANNEL0 + channel;
    
    command = ((channel & 0x3) << 6) | PIT_ACCESS_LATCH;
    
    outb(PIT_COMMAND, command);
    
    low = inb(port);
    high = inb(port);
    
    return (uint16_t)((high << 8) | low);
}

void pit_oneshot(uint32_t microseconds) {
    uint16_t divisor;

    uint64_t ticks = ((uint64_t)PIT_BASE_FREQ * microseconds) / 1000000;
    
    if (ticks > 65535) {
        ticks = 65535;
    } else if (ticks == 0) {
        ticks = 1;
    }
    
    divisor = (uint16_t)ticks;
    
    outb(PIT_COMMAND, PIT_CMD_COUNTER0_ONESHOT);
    
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));
    
    pic_clear_mask(PIT_IRQ);
}

void pit_start(void) {
    pic_clear_mask(PIT_IRQ);
}

void pit_stop(void) {
    pic_set_mask(PIT_IRQ);
}

uint16_t pit_calculate_divisor(uint32_t frequency) {
    uint32_t divisor;
    
    if (frequency == 0) {
        return 65535;
    }
    
    divisor = PIT_BASE_FREQ / frequency;
    
    // Clamp to valid range
    if (divisor > 65535) {
        divisor = 65535;
    } else if (divisor < 1) {
        divisor = 1;
    }
    
    return (uint16_t)divisor;
}

uint32_t pit_get_frequency(void) {
    return pit_frequency;
}

uint64_t pit_get_ticks(void) {
    return pit_ticks;
}

void pit_handler(void) {
    pit_ticks++;
    
}

void pit_delay(uint32_t milliseconds) {
    uint64_t target_ticks;
    uint64_t start_ticks;
    
    target_ticks = ((uint64_t)milliseconds * pit_frequency) / 1000;
    
    start_ticks = pit_ticks;
    
    while ((pit_ticks - start_ticks) < target_ticks) {
        __asm__ volatile("hlt");
    }
}

uint64_t pit_get_uptime_ms(void) {
    if (pit_frequency == 0) {
        return 0;
    }
    return (pit_ticks * 1000) / pit_frequency;
}

uint64_t pit_get_uptime_s(void) {
    if (pit_frequency == 0) {
        return 0;
    }
    return pit_ticks / pit_frequency;
}