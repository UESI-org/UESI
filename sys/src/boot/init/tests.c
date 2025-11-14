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
#include "pmm.h"
#include "proc.h"
#include "elf_loader.h"
#include "paging.h"
#include "proc.h"

#define PROCESS_USER_STACK_SIZE (2 * 1024 * 1024)

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

void test_userland_load(const void *elf_data, size_t elf_size, const char *name) {
    debug_section("Testing Userland Load & Run");

    if (userland_load_and_run(elf_data, elf_size, name)) {
        debug_success("Userland program loaded and entered successfully");
    } else {
        debug_error("Failed to load/enter userland program");
    }
}

bool userland_load_and_run(const void *elf_data, size_t elf_size, const char *name) {
    extern void serial_printf(uint16_t port, const char *fmt, ...);
    extern struct limine_hhdm_response *boot_get_hhdm(void);
    #define DEBUG_PORT 0x3F8
    
    /* Get HHDM offset for address conversion */
    struct limine_hhdm_response *hhdm = boot_get_hhdm();
    uint64_t hhdm_offset = hhdm ? hhdm->offset : 0;
    
    serial_printf(DEBUG_PORT, "\n=== USERLAND_LOAD_AND_RUN START ===\n");
    printf_("\n=== Loading User Program: %s ===\n", name);
    
    serial_printf(DEBUG_PORT, "Creating process...\n");
    process_t *proc = process_create(name);
    if (!proc) {
        serial_printf(DEBUG_PORT, "ERROR: Failed to create process\n");
        printf_("Failed to create process\n");
        return false;
    }
    serial_printf(DEBUG_PORT, "Process created: PID=%d\n", proc->pid);
    
    serial_printf(DEBUG_PORT, "Loading ELF...\n");
    uint64_t entry_point = 0;
    if (!elf_load(proc, elf_data, elf_size, &entry_point)) {
        serial_printf(DEBUG_PORT, "ERROR: ELF load failed\n");
        printf_("Failed to load ELF executable\n");
        process_destroy(proc);
        return false;
    }
    serial_printf(DEBUG_PORT, "ELF loaded, entry=0x%016lx\n", entry_point);
    
    serial_printf(DEBUG_PORT, "Allocating user stack...\n");
    uint64_t stack_top = proc->user_stack_top;
    uint64_t stack_bottom = stack_top - PROCESS_USER_STACK_SIZE;
    size_t stack_pages = PROCESS_USER_STACK_SIZE / 4096;
    
    serial_printf(DEBUG_PORT, "Stack: 0x%016lx - 0x%016lx (%lu pages)\n",
                  stack_bottom, stack_top, (unsigned long)stack_pages);
    
    /* CRITICAL FIX: pmm_alloc returns HHDM virtual, convert to physical */
    for (size_t i = 0; i < stack_pages; i++) {
        uint64_t virt_page = stack_bottom + (i * 4096);
        
        if (i % 128 == 0) {
            serial_printf(DEBUG_PORT, "Allocating page %lu/%lu at 0x%016lx\n",
                          (unsigned long)i, (unsigned long)stack_pages, virt_page);
        }
        
        /* Get HHDM virtual address from PMM */
        void *page_virt = pmm_alloc();
        if (page_virt == NULL) {
            serial_printf(DEBUG_PORT, "ERROR: Failed to allocate physical page %lu\n", 
                          (unsigned long)i);
            printf_("Failed to allocate stack page\n");
            process_destroy(proc);
            return false;
        }
        
        /* Convert to physical address */
        uint64_t phys_page = (uint64_t)page_virt - hhdm_offset;
        
        uint64_t flags = PAGING_FLAG_PRESENT | PAGING_FLAG_WRITE | 
                        PAGING_FLAG_USER | PAGING_FLAG_NX;
        
        if (!paging_map_range(proc->page_dir, virt_page, phys_page, 1, flags)) {
            serial_printf(DEBUG_PORT, "ERROR: Failed to map page %lu at virt=0x%016lx phys=0x%016lx\n",
                          (unsigned long)i, virt_page, phys_page);
            printf_("Failed to map stack page\n");
            pmm_free(page_virt);  /* Free using virtual address */
            process_destroy(proc);
            return false;
        }
    }
    
    serial_printf(DEBUG_PORT, "Stack allocated successfully\n");
    printf_("User stack: 0x%lx - 0x%lx (%lu KB)\n", 
        stack_bottom, stack_top, (unsigned long)(PROCESS_USER_STACK_SIZE / 1024));
    
    serial_printf(DEBUG_PORT, "About to enter usermode...\n");
    printf_("\n=== Entering User Mode ===\n\n");
    
    process_enter_usermode(proc, entry_point, stack_top);
    
    return true;
}