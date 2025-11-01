#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "limine.h"
#include "tty.h"
#include "gdt.h"
#include "isr.h"
#include "idt.h"
#include "pic.h"
#include "serial.h"
#include "serial_debug.h"
#include "keyboard.h"
#include "cpuid.h"
#include "pit.h"
#include "pmm.h"
#include "vmm.h"
#include "mmu.h"
#include "paging.h"

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

static void hcf(void) {
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

static void keyboard_handler(char c) {
    tty_putchar(c);
}

void kmain(void) {
    debug_init();

    if (limine_base_revision[2] != 0) {
        debug_error("Limine base revision mismatch");
        hcf();
    }
    debug_success("Limine protocol verified");

    debug_section("Initializing Physical Memory Manager");
    if (memmap_request.response == NULL || hhdm_request.response == NULL) {
        debug_error("Memory map or HHDM not available");
        hcf();
    }

    pmm_init(memmap_request.response, hhdm_request.response);

    uint64_t total_mem = pmm_get_total_memory();
    uint64_t free_mem = pmm_get_free_memory();
    uint64_t used_mem = total_mem - free_mem;

    debug_success("PMM initialized");
    if (debug_is_enabled()) {
        serial_printf(DEBUG_PORT, "Total Memory: %llu MB\n", total_mem / (1024 * 1024));
        serial_printf(DEBUG_PORT, "Free Memory: %llu MB\n", free_mem / (1024 * 1024));
        serial_printf(DEBUG_PORT, "Used Memory: %llu MB\n", used_mem / (1024 * 1024));
    }

    if (framebuffer_request.response == NULL ||
        framebuffer_request.response->framebuffer_count < 1) {
        debug_error("No framebuffer available");
        hcf();
    }

    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];
    debug_framebuffer_info(framebuffer->width, framebuffer->height,
                          framebuffer->pitch, framebuffer->bpp);

    debug_section("Initializing TTY");
    tty_init(framebuffer);
    tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
    debug_success("TTY initialized");

    tty_set_color(TTY_COLOR_CYAN, TTY_COLOR_BLACK);
    tty_writestring("Physical Memory Manager\n");
    tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
    tty_printf(" Total: %llu MB\n", total_mem / (1024 * 1024));
    tty_printf(" Free: %llu MB\n", free_mem / (1024 * 1024));
    tty_printf(" Used: %llu MB\n", used_mem / (1024 * 1024));
    tty_writestring("\n");

    debug_section("Initializing MMU");
    mmu_init(hhdm_request.response);
    debug_success("MMU initialized");

    debug_section("Initializing Virtual Memory Manager");
    vmm_init();
    debug_success("VMM initialized");

    debug_section("Detecting CPU");
    cpu_info_t cpu;
    cpuid_init(&cpu);

    if (!cpu.cpuid_supported) {
        debug_error("CPUID not supported!");
        tty_set_color(TTY_COLOR_RED, TTY_COLOR_BLACK);
        tty_writestring("ERROR: CPUID instruction not supported\n");
        tty_writestring("This kernel requires a CPU with CPUID support\n");
        hcf();
    }

    tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
    tty_printf("CPU Vendor: %s\n", cpu.vendor);

    char *brand = cpu.brand;
    while (*brand == ' ') brand++;
    if (brand[0] != '\0') {
        tty_printf("CPU Brand: %s\n", brand);
    }

    tty_printf("CPU Family: %u, Model: %u, Stepping: %u\n",
               cpu.family, cpu.model, cpu.stepping);

    tty_writestring("Checking required features:\n");

    if (cpuid_has_long_mode()) {
        tty_set_color(TTY_COLOR_GREEN, TTY_COLOR_BLACK);
        tty_writestring(" [OK] ");
        tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
        tty_writestring("64-bit Long Mode\n");
    } else {
        tty_set_color(TTY_COLOR_RED, TTY_COLOR_BLACK);
        tty_writestring(" [FAIL] ");
        tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
        tty_writestring("64-bit Long Mode not supported!\n");
    }

    if (cpuid_has_pae()) {
        tty_set_color(TTY_COLOR_GREEN, TTY_COLOR_BLACK);
        tty_writestring(" [OK] ");
        tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
        tty_writestring("PAE (Physical Address Extension)\n");
    }

    if (cpuid_has_nx()) {
        tty_set_color(TTY_COLOR_GREEN, TTY_COLOR_BLACK);
        tty_writestring(" [OK] ");
        tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
        tty_writestring("NX (No-Execute bit)\n");
    }

    if (cpuid_has_apic()) {
        tty_set_color(TTY_COLOR_GREEN, TTY_COLOR_BLACK);
        tty_writestring(" [OK] ");
        tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
        tty_writestring("APIC (Advanced PIC)\n");
    }

    if (cpuid_has_msr()) {
        tty_set_color(TTY_COLOR_GREEN, TTY_COLOR_BLACK);
        tty_writestring(" [OK] ");
        tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
        tty_writestring("MSR (Model Specific Registers)\n");
    }

    tty_writestring("Optional features:\n");

    if (cpuid_has_sse2()) {
        tty_set_color(TTY_COLOR_CYAN, TTY_COLOR_BLACK);
        tty_writestring(" [+] ");
        tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
        tty_writestring("SSE2\n");
    }

    if (cpuid_has_sse3()) {
        tty_set_color(TTY_COLOR_CYAN, TTY_COLOR_BLACK);
        tty_writestring(" [+] ");
        tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
        tty_writestring("SSE3\n");
    }

    if (cpuid_has_avx()) {
        tty_set_color(TTY_COLOR_CYAN, TTY_COLOR_BLACK);
        tty_writestring(" [+] ");
        tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
        tty_writestring("AVX\n");
    }

    if (cpuid_has_1gb_pages()) {
        tty_set_color(TTY_COLOR_CYAN, TTY_COLOR_BLACK);
        tty_writestring(" [+] ");
        tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
        tty_writestring("1GB Pages\n");
    }

    debug_success("CPU detection complete");

    debug_section("Initializing GDT");
    gdt_init();
    tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
    tty_writestring("GDT initialized\n");
    tty_writestring(" - Null descriptor (entry 0)\n");
    tty_writestring(" - Kernel code/data segments (Ring 0)\n");
    tty_writestring(" - User code/data segments (Ring 3)\n");
    tty_writestring(" - TSS descriptor (privilege switching)\n");
    debug_success("GDT initialized with 5 segments + TSS");

    debug_section("Initializing IDT");
    idt_init();
    isr_install();
    tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
    tty_writestring("IDT initialized\n");
    tty_writestring(" - Exception handlers (INT 0-31)\n");
    tty_writestring(" - IRQ handlers (INT 32-47)\n");
    debug_success("IDT initialized");

    debug_section("Initializing PIC");
    pic_init();
    uint16_t mask = pic_get_mask();
    tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
    tty_printf("PIC initialized and remapped\n");
    tty_printf(" Master PIC: IRQ 0-7 -> INT 32-39\n");
    tty_printf(" Slave PIC: IRQ 8-15 -> INT 40-47\n");
    tty_printf(" Initial mask: 0x%x\n", mask);
    debug_success("PIC initialized and remapped");

    debug_section("Initializing PIT");
    uint32_t actual_freq = pit_init(100);
    tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
    tty_printf("PIT initialized\n");
    tty_printf(" Target frequency: 100 Hz\n");
    tty_printf(" Actual frequency: %u Hz (%u ms tick)\n", actual_freq, 1000/actual_freq);
    tty_printf(" PIT connected to IRQ0 (INT 32)\n");
    debug_success("PIT initialized and enabled");
    isr_register_handler(32, (isr_handler_t)pit_handler);
    debug_success("PIT initialized and handler registered");

    pic_clear_mask(IRQ_KEYBOARD);
    debug_success("Keyboard IRQ enabled (IRQ1)");

    debug_section("Initializing Keyboard");
    keyboard_init();
    keyboard_set_callback(keyboard_handler);
    tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
    tty_writestring("Keyboard initialized (QWERTZ layout)\n");
    debug_success("Keyboard initialized");

    debug_banner("Kernel Initialization Complete");
    tty_writestring("\n");

    tty_set_color(TTY_COLOR_CYAN, TTY_COLOR_BLACK);
    tty_writestring("=== System Status ===\n");
    tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);

    tty_printf("CPU: %s", cpu.vendor);
    if (brand[0] != '\0') {
        tty_printf(" (%s)", brand);
    }
    tty_writestring("\n");

    mask = pic_get_mask();
    tty_printf("Active IRQ mask: 0x%x\n", mask);
    tty_printf("Enabled IRQs: ");

    bool first = true;
    for (int i = 0; i < 16; i++) {
        if (!(mask & (1 << i))) {
            if (!first) tty_writestring(", ");
            tty_printf("%d", i);
            first = false;
        }
    }
    tty_writestring("\n\n");

    __asm__ volatile("sti");

    tty_set_color(TTY_COLOR_GREEN, TTY_COLOR_BLACK);
    tty_writestring("System ready. Type something!\n");
    tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
    tty_writestring("> ");

    if (debug_is_enabled()) {
        serial_printf(DEBUG_PORT, "\n=== System Ready ===\n");
        serial_printf(DEBUG_PORT, "CPU: %s (%s)\n", cpu.vendor, brand);
        serial_printf(DEBUG_PORT, "GDT loaded into GDTR\n");
        serial_printf(DEBUG_PORT, "IDT loaded into IDTR\n");
        serial_printf(DEBUG_PORT, "TSS loaded into TR\n");
        serial_printf(DEBUG_PORT, "IRQ mask: 0x%x\n", mask);
        serial_printf(DEBUG_PORT, "Interrupts enabled (STI)\n");
        serial_printf(DEBUG_PORT, "Awaiting keyboard input...\n");
    }

    for (;;) {
        __asm__ volatile("hlt");
    }
}
