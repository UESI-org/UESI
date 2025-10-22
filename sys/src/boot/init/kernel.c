#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "limine.h"
#include "tty.h"
#include "gdt.h"
#include "isr.h"
#include "idt.h"
#include "serial.h"

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
    bool serial_ok = serial_init(SERIAL_COM1);
    
    if (serial_ok) {
        serial_write_string(SERIAL_COM1, "\n=== UESI Kernel Boot ===\n");
        serial_write_string(SERIAL_COM1, "Serial port initialized (COM1, 115200 baud)\n");
    }
    
    if (limine_base_revision[2] != 0) {
        if (serial_ok) {
            serial_write_string(SERIAL_COM1, "ERROR: Limine base revision mismatch\n");
        }
        hcf();
    }
    
    if (serial_ok) {
        serial_write_string(SERIAL_COM1, "Limine protocol verified\n");
    }
    
    if (framebuffer_request.response == NULL ||
        framebuffer_request.response->framebuffer_count < 1) {
        if (serial_ok) {
            serial_write_string(SERIAL_COM1, "ERROR: No framebuffer available\n");
        }
        hcf();
    }
    
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];
    
    if (serial_ok) {
        serial_printf(SERIAL_COM1, "Framebuffer: %ux%u, pitch=%u, bpp=%u\n",
                     framebuffer->width, framebuffer->height,
                     framebuffer->pitch, framebuffer->bpp);
    }
    
    tty_init(framebuffer);
    tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
    tty_writestring("UESI - V1\n");
    
    if (serial_ok) {
        serial_write_string(SERIAL_COM1, "TTY initialized\n");
    }
    
    tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLUE);
    
    gdt_init();
    tty_writestring("GDT initialized\n");
    
    if (serial_ok) {
        serial_write_string(SERIAL_COM1, "GDT initialized\n");
    }
    
    idt_init();
    isr_install();
    tty_writestring("IDT initialized\n");
    
    if (serial_ok) {
        serial_write_string(SERIAL_COM1, "IDT initialized\n");
        serial_write_string(SERIAL_COM1, "Kernel initialization complete\n");
        serial_write_string(SERIAL_COM1, "=== System halted ===\n");
    }
    
    hcf();
}