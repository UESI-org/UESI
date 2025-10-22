#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "limine.h"
#include "tty.h"
#include "gdt.h"
#include "isr.h"
#include "idt.h"
#include "serial.h"
#include "serial_debug.h"

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

void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t *d = dest;
    const uint8_t *s = src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dest;
}

void *memset(void *s, int c, size_t n) {
    uint8_t *p = s;
    for (size_t i = 0; i < n; i++) p[i] = (uint8_t)c;
    return s;
}

void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *d = dest;
    const uint8_t *s = src;
    if (d < s) {
        for (size_t i = 0; i < n; i++) d[i] = s[i];
    } else {
        for (size_t i = n; i > 0; i--) d[i-1] = s[i-1];
    }
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *a = s1;
    const uint8_t *b = s2;
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    }
    return 0;
}

static void hcf(void) {
    for (;;) __asm__ volatile ("hlt");
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
    tty_writestring("UESI - V1\n");
    debug_success("TTY initialized");
    
    tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLUE);
    
    debug_section("Initializing GDT");
    gdt_init();
    tty_writestring("GDT initialized\n");
    debug_success("GDT initialized");
    
    debug_section("Initializing IDT");
    idt_init();
    isr_install();
    tty_writestring("IDT initialized\n");
    debug_success("IDT initialized");
    
    debug_banner("Kernel Initialization Complete");
    
    if (debug_is_enabled()) {
        serial_printf(DEBUG_PORT, "System halted at entry point\n");
        serial_printf(DEBUG_PORT, "Ready for execution\n");
    }
    
    hcf();
}