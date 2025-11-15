#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "limine.h"
#include "tty.h"
#include "gdt.h"
#include "isr.h"
#include "idt.h"
#include "pic.h"
#include "pci.h"
#include "serial_debug.h"
#include "scheduler.h"
#include "syscall.h"
#include "keyboard.h"
#include "kdebug.h"
#include "cpuid.h"
#include "pit.h"
#include "pmm.h"
#include "vmm.h"
#include "mmu.h"
#include "printf.h"
#include "proc.h"
#include "kmalloc.h"
#include "rtc.h"
#include "boot.h"
#include "tests.h"
#include "system_info.h"

static void hcf(void) {
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

static void keyboard_handler(char c) {
    tty_putchar(c);
}

static void initialize_memory(void) {
    debug_section("Initializing Memory Management");
    
    struct limine_memmap_response *memmap = boot_get_memmap();
    struct limine_hhdm_response *hhdm = boot_get_hhdm();
    
    if (memmap == NULL || hhdm == NULL) {
        debug_error("Memory map or HHDM not available");
        hcf();
    }

    pmm_init(memmap, hhdm);
    debug_success("PMM initialized");

    kmalloc_init();
    debug_success("kmalloc initialized");

    mmu_init(hhdm);
    debug_success("MMU initialized");

    vmm_init();
    debug_success("VMM initialized");
}

static void initialize_cpu(cpu_info_t *cpu) {
    debug_section("Detecting CPU");
    cpuid_init(cpu);

    if (!cpu->cpuid_supported) {
        debug_error("CPUID not supported!");
        tty_set_color(TTY_COLOR_RED, TTY_COLOR_BLACK);
        printf("FATAL: CPUID instruction not supported\n");
        hcf();
    }

    bool all_features_present = true;
    
    if (!cpuid_has_long_mode()) {
        tty_set_color(TTY_COLOR_RED, TTY_COLOR_BLACK);
        printf("FATAL: 64-bit Long Mode not supported\n");
        all_features_present = false;
    }
    
    if (!cpuid_has_pae()) {
        tty_set_color(TTY_COLOR_RED, TTY_COLOR_BLACK);
        printf("FATAL: PAE not supported\n");
        all_features_present = false;
    }
    
    if (!cpuid_has_nx()) {
        tty_set_color(TTY_COLOR_RED, TTY_COLOR_BLACK);
        printf("FATAL: NX bit not supported\n");
        all_features_present = false;
    }

    if (!all_features_present) {
        tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
        hcf();
    }

    debug_success("CPU detection complete");
    
    if (debug_is_enabled()) {
        system_print_cpu_info(cpu);
    }
}

static void initialize_system_components(void) {
    debug_section("Initializing System Components");
    
    gdt_init();
    debug_success("GDT initialized");

    idt_init();
    isr_install();
    debug_success("IDT initialized");

    kdebug_init();
    isr_register_handler(3, kdebug_breakpoint_handler);  // INT3 for breakpoints
    debug_success("Kernel debugger initialized");

    syscall_init();

    pic_init();
    debug_success("PIC initialized");

    rtc_init();
    debug_success("RTC initialized");

    pit_init(100);
    isr_register_handler(32, (isr_handler_t)pit_handler);
    debug_success("PIT initialized (100 Hz)");

    scheduler_init(100);
    debug_success("Scheduler initialized");

    keyboard_init();
    keyboard_set_callback(keyboard_handler);
    pic_clear_mask(IRQ_KEYBOARD);
    debug_success("Keyboard initialized");
}

static void run_tests(void) {
    test_kmalloc();
    test_rtc();
    
    struct limine_module_response *modules = boot_get_modules();
    if (modules && modules->module_count > 0) {
        struct limine_file *user_prog = modules->modules[0];
        debug_section("Loading Userland Program");
        printf("Module: %s (%lu bytes)\n", 
               user_prog->path, (unsigned long)user_prog->size);
        userland_load_and_run(user_prog->address, user_prog->size, "test");
    } else {
        printf("No userland modules found\n");
    }
}

void kmain(void) {
    debug_init();

    if (!boot_verify_protocol()) {
        debug_error("Limine base revision mismatch");
        hcf();
    }
    debug_success("Limine protocol verified");

    boot_init();

    struct limine_framebuffer *framebuffer = boot_get_framebuffer();
    if (framebuffer == NULL) {
        debug_error("No framebuffer available");
        hcf();
    }

    debug_framebuffer_info(framebuffer->width, framebuffer->height,
                          framebuffer->pitch, framebuffer->bpp);

    tty_init(framebuffer);
    tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
    debug_success("TTY initialized");

    initialize_memory();

    cpu_info_t cpu;
    initialize_cpu(&cpu);

    debug_section("Initializing PCI");
    pci_init();
    pci_print_devices();

    initialize_system_components();

    process_init();
    debug_success("Process subsystem initialized");

    run_tests();

    debug_banner("Kernel Initialization Complete");

    system_print_banner(&cpu);

    __asm__ volatile("sti");

    tty_set_color(TTY_COLOR_GREEN, TTY_COLOR_BLACK);
    printf("System ready. Type something!\n");
    tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
    printf("> ");

    if (debug_is_enabled()) {
        serial_printf(DEBUG_PORT, "\n=== System Ready ===\n");
        serial_printf(DEBUG_PORT, "Interrupts enabled\n");
        serial_printf(DEBUG_PORT, "Awaiting input...\n");
    }

    for (;;) {
        __asm__ volatile("hlt");
    }
}