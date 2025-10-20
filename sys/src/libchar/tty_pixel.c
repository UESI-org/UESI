#include "tty.h"

void tty_putpixel(uint64_t x, uint64_t y, uint32_t color) {
    tty_t *tty = tty_get();
    
    if (x >= tty->width || y >= tty->height)
        return;
    
    uint64_t offset = y * (tty->pitch / 4) + x;
    tty->buffer[offset] = color;
}

uint32_t tty_getpixel(uint64_t x, uint64_t y) {
    tty_t *tty = tty_get();
    
    if (x >= tty->width || y >= tty->height)
        return 0;
    
    uint64_t offset = y * (tty->pitch / 4) + x;
    return tty->buffer[offset];
}

void tty_fill_rect(uint64_t x, uint64_t y, uint64_t w, uint64_t h, uint32_t color) {
    for (uint64_t dy = 0; dy < h; dy++) {
        for (uint64_t dx = 0; dx < w; dx++) {
            tty_putpixel(x + dx, y + dy, color);
        }
    }
}