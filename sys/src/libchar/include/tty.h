#ifndef TTY_H
#define TTY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "limine.h"

#define TTY_COLOR_BLACK         0x00000000
#define TTY_COLOR_BLUE          0x000000AA
#define TTY_COLOR_GREEN         0x0000AA00
#define TTY_COLOR_CYAN          0x0000AAAA
#define TTY_COLOR_RED           0x00AA0000
#define TTY_COLOR_MAGENTA       0x00AA00AA
#define TTY_COLOR_BROWN         0x00AA5500
#define TTY_COLOR_LIGHT_GRAY    0x00AAAAAA
#define TTY_COLOR_DARK_GRAY     0x00555555
#define TTY_COLOR_LIGHT_BLUE    0x005555FF
#define TTY_COLOR_LIGHT_GREEN   0x0055FF55
#define TTY_COLOR_LIGHT_CYAN    0x0055FFFF
#define TTY_COLOR_LIGHT_RED     0x00FF5555
#define TTY_COLOR_LIGHT_MAGENTA 0x00FF55FF
#define TTY_COLOR_YELLOW        0x00FFFF55
#define TTY_COLOR_WHITE         0x00FFFFFF

typedef struct {
    struct limine_framebuffer *fb;
    uint32_t *buffer;
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint64_t bpp;
    
    uint64_t cursor_x;
    uint64_t cursor_y;
    uint64_t char_width;
    uint64_t char_height;
    uint64_t cols;
    uint64_t rows;
    
    uint32_t fg_color;
    uint32_t bg_color;
    
    bool cursor_visible;
} tty_t;

void tty_init(struct limine_framebuffer *framebuffer);
tty_t *tty_get(void);

void tty_putchar(char c);
void tty_write(const char *str, size_t len);
void tty_writestring(const char *str);

void tty_printf(const char *fmt, ...);

void tty_set_cursor(uint64_t x, uint64_t y);
void tty_get_cursor(uint64_t *x, uint64_t *y);
void tty_show_cursor(bool visible);
void tty_draw_cursor(void);

void tty_set_color(uint32_t fg, uint32_t bg);
void tty_get_color(uint32_t *fg, uint32_t *bg);

void tty_clear(void);
void tty_scroll(void);

void tty_putpixel(uint64_t x, uint64_t y, uint32_t color);
uint32_t tty_getpixel(uint64_t x, uint64_t y);

void tty_fill_rect(uint64_t x, uint64_t y, uint64_t w, uint64_t h, uint32_t color);

void tty_draw_char(char c, uint64_t x, uint64_t y, uint32_t fg, uint32_t bg);

#endif