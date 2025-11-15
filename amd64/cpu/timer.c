#include <timer.h>
#include <idt.h>
#include <gdt.h>
#include <io.h>
#include <stddef.h>

static volatile uint64_t timer_ticks = 0;
static uint32_t timer_frequency = TIMER_FREQ_HZ;
static uint32_t timer_divisor = 0;
static timer_callback_node_t *callback_list = NULL;
static volatile uint64_t interrupt_count = 0;

extern void timer_interrupt_handler(void);
extern void timer_handler(void);

void timer_handler(void) {
    timer_ticks++;
    interrupt_count++;
    
    if (callback_list == NULL) {
        return;
    }
    
    timer_callback_node_t *current = callback_list;
    timer_callback_node_t *prev = NULL;
    
    while (current != NULL) {
        if (!current->active) {
            if (prev == NULL) {
                callback_list = current->next;
                current = callback_list;
            } else {
                prev->next = current->next;
                current = current->next;
            }
            continue;
        }
        
        if (timer_ticks >= current->fire_tick) {
            current->callback(current->data);
            
            if (current->interval > 0) {
                current->fire_tick = timer_ticks + current->interval;
            } else {
                current->active = 0;
            }
        }
        
        prev = current;
        current = current->next;
    }
}

void timer_set_frequency(uint32_t frequency_hz) {
    if (frequency_hz < 18 || frequency_hz > PIT_FREQUENCY) {
        frequency_hz = TIMER_FREQ_HZ;
    }
    
    timer_frequency = frequency_hz;
    timer_divisor = PIT_FREQUENCY / frequency_hz;
    
    uint8_t command = PIT_CHANNEL_0 | PIT_ACCESS_LOHI | PIT_MODE_2 | PIT_BINARY;
    outb(PIT_COMMAND, command);
    
    outb(PIT_CHANNEL0, (uint8_t)(timer_divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((timer_divisor >> 8) & 0xFF));
}

void timer_init(uint32_t frequency_hz) {
    timer_ticks = 0;
    interrupt_count = 0;
    callback_list = NULL;
    
    timer_set_frequency(frequency_hz);
    
    idt_set_gate(32 + TIMER_IRQ, (uint64_t)timer_interrupt_handler, 
                 GDT_SELECTOR_KERNEL_CODE, IDT_GATE_INTERRUPT, 0);
}

uint64_t timer_get_ticks(void) {
    return timer_ticks;
}

uint64_t timer_get_uptime_ms(void) {
    return (timer_ticks * 1000) / timer_frequency;
}

uint64_t timer_get_uptime_sec(void) {
    return timer_ticks / timer_frequency;
}

void timer_delay_ticks(uint64_t ticks) {
    uint64_t end = timer_ticks + ticks;
    while (timer_ticks < end) {
        __asm__ volatile("hlt");
    }
}

void timer_delay_ms(uint32_t ms) {
    uint64_t ticks = (ms * timer_frequency) / 1000;
    timer_delay_ticks(ticks);
}

void timer_sleep_ms(uint32_t ms) {
    timer_delay_ms(ms);
}

static timer_callback_node_t *alloc_callback_node(void) {
    static timer_callback_node_t pool[64];
    static uint32_t next_free = 0;
    
    if (next_free >= 64) {
        return NULL;
    }
    
    return &pool[next_free++];
}

timer_callback_node_t *timer_register_oneshot(timer_callback_t callback, void *data, uint32_t delay_ms) {
    if (callback == NULL) {
        return NULL;
    }
    
    timer_callback_node_t *node = alloc_callback_node();
    if (node == NULL) {
        return NULL;
    }
    
    uint64_t delay_ticks = (delay_ms * timer_frequency) / 1000;
    if (delay_ticks == 0) {
        delay_ticks = 1;
    }
    
    node->callback = callback;
    node->data = data;
    node->fire_tick = timer_ticks + delay_ticks;
    node->interval = 0;
    node->active = 1;
    node->next = callback_list;
    callback_list = node;
    
    return node;
}

timer_callback_node_t *timer_register_periodic(timer_callback_t callback, void *data, uint32_t interval_ms) {
    if (callback == NULL || interval_ms == 0) {
        return NULL;
    }
    
    timer_callback_node_t *node = alloc_callback_node();
    if (node == NULL) {
        return NULL;
    }
    
    uint64_t interval_ticks = (interval_ms * timer_frequency) / 1000;
    if (interval_ticks == 0) {
        interval_ticks = 1;
    }
    
    node->callback = callback;
    node->data = data;
    node->fire_tick = timer_ticks + interval_ticks;
    node->interval = interval_ticks;
    node->active = 1;
    node->next = callback_list;
    callback_list = node;
    
    return node;
}

void timer_cancel_callback(timer_callback_node_t *node) {
    if (node != NULL) {
        node->active = 0;
    }
}

uint32_t timer_get_frequency(void) {
    return timer_frequency;
}

void timer_get_stats(timer_stats_t *stats) {
    if (stats == NULL) {
        return;
    }
    
    stats->total_ticks = timer_ticks;
    stats->total_interrupts = interrupt_count;
    stats->frequency_hz = timer_frequency;
    
    uint32_t active = 0;
    timer_callback_node_t *current = callback_list;
    while (current != NULL) {
        if (current->active) {
            active++;
        }
        current = current->next;
    }
    stats->active_callbacks = active;
}