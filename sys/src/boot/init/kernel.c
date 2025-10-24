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

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

static void hcf(void) {
    for (;;) __asm__ volatile ("hlt");
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

    debug_section("Initializing GDT");
    gdt_init();
    tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
    tty_writestring("GDT initialized\n");
    debug_success("GDT initialized");

    debug_section("Initializing IDT");
    idt_init();
    isr_install();
    tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
    tty_writestring("IDT initialized\n");
    debug_success("IDT initialized");

    debug_section("Initializing PIC");
    pic_init();
    
    uint16_t mask = pic_get_mask();
    tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
    tty_printf("PIC initialized and remapped\n");
    tty_printf("  Master PIC: IRQ 0-7  -> INT 32-39\n");
    tty_printf("  Slave PIC:  IRQ 8-15 -> INT 40-47\n");
    tty_printf("  Initial mask: 0x%x\n", mask);
    
    debug_success("PIC initialized and remapped");
    
    pic_clear_mask(IRQ_KEYBOARD);
    debug_success("Keyboard IRQ enabled");

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
        serial_printf(DEBUG_PORT, "IRQ mask: 0x%x\n", mask);
        serial_printf(DEBUG_PORT, "Awaiting keyboard input...\n");
    }

    for (;;) {
        __asm__ volatile("hlt");
    }
}