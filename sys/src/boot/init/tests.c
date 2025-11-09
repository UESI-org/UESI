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
    time_t timestamp;
    
    if (rtc_get_time(&time) == 0) {
        debug_success("RTC read successful (UTC)");
        serial_printf(DEBUG_PORT, "UTC time: %04d-%02d-%02d %02d:%02d:%02d\n",
                      time.year, time.month, time.day,
                      time.hour, time.minute, time.second);
        tty_set_color(TTY_COLOR_YELLOW, TTY_COLOR_BLACK);
        printf("UTC Time: %04d-%02d-%02d %02d:%02d:%02d\n",
               time.year, time.month, time.day,
               time.hour, time.minute, time.second);
        tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
    } else {
        debug_error("Failed to read RTC");
        return;
    }
    
    rtc_set_timezone(TZ_CET);
    timestamp = rtc_get_timestamp();
    if (rtc_get_time_tz(&time, TZ_CET) == 0) {
        debug_success("Berlin time read successful");
        tty_set_color(TTY_COLOR_GREEN, TTY_COLOR_BLACK);
        printf("Berlin Time: %04d-%02d-%02d %02d:%02d:%02d (timestamp: %lu)\n",
               time.year, time.month, time.day,
               time.hour, time.minute, time.second, (unsigned long)timestamp);
        tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
    }
    
    tty_set_color(TTY_COLOR_CYAN, TTY_COLOR_BLACK);
    printf("\n--- World Clock ---\n");
    tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
    
    const struct {
        timezone_t tz;
        const char *name;
    } zones[] = {
        {TZ_UTC,  "UTC      "},
        {TZ_CET,  "Berlin   "},
        {TZ_EST,  "New York "},
        {TZ_PST,  "Los Angeles"},
        {TZ_JST,  "Tokyo    "},
        {TZ_AEST, "Sydney   "}
    };
    
    for (int i = 0; i < 6; i++) {
        if (rtc_get_time_tz(&time, zones[i].tz) == 0) {
            printf("%s: %02d:%02d:%02d\n",
                   zones[i].name,
                   time.hour, time.minute, time.second);
        }
    }
    
    printf("\n--- Timezone Offsets ---\n");
    rtc_time_t utc_time;
    rtc_get_time_tz(&utc_time, TZ_UTC);
    
    time_t utc_ts = rtc_get_timestamp_tz(TZ_UTC);
    time_t berlin_ts = rtc_get_timestamp_tz(TZ_CET);
    int offset_hours = (berlin_ts - utc_ts) / 3600;
    
    printf("UTC timestamp: %lu\n", (unsigned long)utc_ts);
    printf("Berlin timestamp: %lu\n", (unsigned long)berlin_ts);
    printf("Offset: %+d hours\n", offset_hours);
    
    timezone_t current = rtc_get_timezone();
    const char *tz_names[] = {"UTC", "CET", "EST", "PST", "JST", "AEST"};
    printf("Current timezone: %s\n", tz_names[current]);
    
    if (time.month >= 3 && time.month <= 10) {
        tty_set_color(TTY_COLOR_YELLOW, TTY_COLOR_BLACK);
        printf("\n[INFO] Possible DST period for some timezones\n");
        tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
    }
    
    debug_success("RTC timezone tests completed");
}