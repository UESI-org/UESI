#include <sys/ahci.h>
#include <tests.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <kmalloc.h>
#include <kfree.h>
#include <rtc.h>
#include <serial.h>
#include <serial_debug.h>
#include <tty.h>
#include <printf.h>
#include <pmm.h>
#include <proc.h>
#include <elf_loader.h>
#include <paging.h>
#include <panic.h>

void test_kmalloc(void) {
    void *ptr1 = kmalloc(128);
    if (ptr1) {
        strcpy((char *)ptr1, "Hello from kmalloc!");
        kfree(ptr1);
    }
    
    void *ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = kmalloc(64);
        if (ptrs[i]) {
            memset(ptrs[i], i, 64);
        }
    }
    
    for (int i = 0; i < 10; i += 2) {
        kfree(ptrs[i]);
    }
    
    for (int i = 1; i < 10; i += 2) {
        kfree(ptrs[i]);
    }
    
    void *small = kmalloc(16);
    void *medium = kmalloc(256);
    void *large = kmalloc(4096);
    
    kfree(small);
    kfree(medium);
    kfree(large);
    
    void *zeroed = kmalloc_flags(512, KMALLOC_ZERO);
    if (zeroed) {
        bool all_zero = true;
        for (int i = 0; i < 512; i++) {
            if (((uint8_t *)zeroed)[i] != 0) {
                all_zero = false;
                break;
            }
        }
        if (!all_zero) {
            debug_error("KMALLOC_ZERO failed to zero memory");
        }
        kfree(zeroed);
    }
    
    debug_success("kmalloc tests passed");
}

void test_ahci(void) {
    extern ahci_controller_t *ahci_controllers;
    
    if (!ahci_controllers) {
        printf("No AHCI controllers to test\n");
        return;
    }
    
    printf("Testing AHCI...\n");
    
    ahci_controller_t *ctrl = ahci_controllers;
    
    for (int i = 0; i < AHCI_MAX_PORTS; i++) {
        if (ctrl->ports[i].implemented &&
            ctrl->ports[i].device_type == AHCI_DEV_SATA) {
            
            void *buffer = kmalloc(512);
            if (!buffer) {
                printf("Failed to allocate buffer\n");
                return;
            }
            
            printf("Reading sector 0 from port %d...\n", i);
            if (ahci_port_read(&ctrl->ports[i], 0, 1, buffer) == 0) {
                printf("Successfully read sector 0\n");
                uint8_t *data = (uint8_t *)buffer;
                printf("First 16 bytes: ");
                for (int j = 0; j < 16; j++) {
                    printf("%02x ", data[j]);
                }
                printf("\n");
            } else {
                printf("Failed to read sector 0\n");
            }
            
            kfree(buffer);
            break;
        }
    }
}

void test_rtc(void) {
    rtc_time_t time;
    time_t timestamp;
    
    if (rtc_get_time(&time) != 0) {
        debug_error("Failed to read RTC");
        return;
    }
    
    tty_set_color(TTY_COLOR_YELLOW, TTY_COLOR_BLACK);
    printf("UTC: %04d-%02d-%02d %02d:%02d:%02d\n",
           time.year, time.month, time.day,
           time.hour, time.minute, time.second);
    tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
    
    rtc_set_timezone(TZ_CET);
    timestamp = rtc_get_timestamp();
    if (rtc_get_time_tz(&time, TZ_CET) == 0) {
        tty_set_color(TTY_COLOR_GREEN, TTY_COLOR_BLACK);
        printf("Berlin: %04d-%02d-%02d %02d:%02d:%02d\n",
               time.year, time.month, time.day,
               time.hour, time.minute, time.second);
        tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
    }
    
    printf("\n");
}

void test_userland_load(const void *elf_data, size_t elf_size, const char *name) {
    if (userland_load_and_run(elf_data, elf_size, name)) {
        debug_success("Userland program loaded successfully");
    } else {
        debug_error("Failed to load userland program");
    }
}

bool userland_load_and_run(const void *elf_data, size_t elf_size, const char *name) {
    extern struct limine_hhdm_response *boot_get_hhdm(void);
    
    struct limine_hhdm_response *hhdm = boot_get_hhdm();
    uint64_t hhdm_offset = hhdm ? hhdm->offset : 0;
    
    printf_("\n=== Loading User Program: %s ===\n", name);
    
    struct process *ps = process_alloc(name);
    if (!ps) {
        printf_("Failed to create process\n");
        return false;
    }
    
    struct proc *p = proc_alloc(ps, name);
    if (!p) {
        printf_("Failed to create main thread\n");
        process_release(ps);
        return false;
    }
    
    uint64_t entry_point = 0;
    if (!elf_load(ps, elf_data, elf_size, &entry_point)) {
        printf_("Failed to load ELF executable\n");
        proc_free(p);
        process_release(ps);
        return false;
    }
    
    uint64_t stack_top = p->p_ustack_top;
    uint64_t stack_bottom = stack_top - PROCESS_USER_STACK_SIZE;
    size_t stack_pages = PROCESS_USER_STACK_SIZE / 4096;
    
    for (size_t i = 0; i < stack_pages; i++) {
        uint64_t virt_page = stack_bottom + (i * 4096);
        
        void *page_virt = pmm_alloc();
        if (page_virt == NULL) {
            serial_printf(DEBUG_PORT, "ERROR: Failed to allocate physical page %lu\n", 
                          (unsigned long)i);
            printf_("Failed to allocate stack page\n");
            proc_free(p);
            process_release(ps);
            return false;
        }
        
        uint64_t phys_page = (uint64_t)page_virt - hhdm_offset;
        
        uint64_t flags = PAGING_FLAG_PRESENT | PAGING_FLAG_WRITE | 
                        PAGING_FLAG_USER | PAGING_FLAG_NX;
        
        if (!paging_map_range(ps->ps_vmspace, virt_page, phys_page, 1, flags)) {
            serial_printf(DEBUG_PORT, "ERROR: Failed to map page at virt=0x%016lx\n", virt_page);
            printf_("Failed to map stack page\n");
            pmm_free(page_virt);
            proc_free(p);
            process_release(ps);
            return false;
        }
    }
    
    printf_("User stack: 0x%lx - 0x%lx (%lu KB)\n", 
        stack_bottom, stack_top, (unsigned long)(PROCESS_USER_STACK_SIZE / 1024));
    
    p->p_stat = SRUN;
    
    printf_("\n=== Entering User Mode ===\n\n");
    
    proc_enter_usermode(p, entry_point, stack_top);
    
    return true;
}