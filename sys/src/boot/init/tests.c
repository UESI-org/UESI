#include "tests.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "kmalloc.h"
#include "kfree.h"
#include "rtc.h"
#include "serial.h"
#include "serial_debug.h"
#include "tty.h"
#include "printf.h"

void test_kmalloc(void) {
    debug_section("Testing kmalloc/kfree");
    
    void *ptr1 = kmalloc(128);
    if (ptr1) {
        debug_success("Allocated 128 bytes");
        strcpy((char *)ptr1, "Hello from kmalloc!");
        serial_printf(DEBUG_PORT, "String: %s\n", (char *)ptr1);
        kfree(ptr1);
        debug_success("Freed 128 bytes");
    } else {
        debug_error("Failed to allocate 128 bytes");
    }
    
    void *ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = kmalloc(64);
        if (ptrs[i]) {
            memset(ptrs[i], i, 64);
        }
    }
    debug_success("Allocated 10x64 bytes");
    
    for (int i = 0; i < 10; i += 2) {
        kfree(ptrs[i]);
    }
    debug_success("Freed 5 allocations");
    
    for (int i = 1; i < 10; i += 2) {
        kfree(ptrs[i]);
    }
    debug_success("Freed remaining allocations");
    
    void *small = kmalloc(16);
    void *medium = kmalloc(256);
    void *large = kmalloc(4096);
    debug_success("Allocated various sizes (16, 256, 4096)");
    
    kfree(small);
    kfree(medium);
    kfree(large);
    debug_success("Freed all sizes");
    
    void *zeroed = kmalloc_flags(512, KMALLOC_ZERO);
    if (zeroed) {
        bool all_zero = true;
        for (int i = 0; i < 512; i++) {
            if (((uint8_t *)zeroed)[i] != 0) {
                all_zero = false;
                break;
            }
        }
        if (all_zero) {
            debug_success("KMALLOC_ZERO works correctly");
        } else {
            debug_error("KMALLOC_ZERO failed to zero memory");
        }
        kfree(zeroed);
    }
    
    if (debug_is_enabled()) {
        kmalloc_stats();
    }
}

void test_rtc(void) {
    debug_section("Testing RTC");
    
    rtc_time_t time;
    if (rtc_get_time(&time) == 0) {
        debug_success("RTC read successful");
        serial_printf(DEBUG_PORT, "Current time: %04d-%02d-%02d %02d:%02d:%02d\n",
                      time.year, time.month, time.day,
                      time.hour, time.minute, time.second);
        
        tty_set_color(TTY_COLOR_YELLOW, TTY_COLOR_BLACK);
        printf("RTC Time: %04d-%02d-%02d %02d:%02d:%02d\n",
               time.year, time.month, time.day,
               time.hour, time.minute, time.second);
        tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
    } else {
        debug_error("Failed to read RTC");
    }
}