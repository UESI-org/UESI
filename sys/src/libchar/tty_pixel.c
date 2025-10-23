#include "tty.h"

void tty_putpixel(uint64_t x, uint64_t y, uint32_t color) {
    tty_t *tty = tty_get();
    
    // Bounds check
    if (x >= tty->width || y >= tty->height) {
        return;
    }
    
    if (!tty->buffer) {
        return;
    }
    
    uint64_t offset = y * (tty->pitch / 4) + x;
    
    // Verify offset is within reasonable bounds
    uint64_t max_offset = (tty->height * tty->pitch) / 4;
    if (offset >= max_offset) {
        return;
    }
    
    tty->buffer[offset] = color;
}

uint32_t tty_getpixel(uint64_t x, uint64_t y) {
    tty_t *tty = tty_get();
    
    // Bounds check
    if (x >= tty->width || y >= tty->height) {
        return 0;
    }
    
    // Safety check
    if (!tty->buffer) {
        return 0;
    }
    
    uint64_t offset = y * (tty->pitch / 4) + x;
    
    // Verify offset
    uint64_t max_offset = (tty->height * tty->pitch) / 4;
    if (offset >= max_offset) {
        return 0;
    }
    
    return tty->buffer[offset];
}

void tty_fill_rect(uint64_t x, uint64_t y, uint64_t w, uint64_t h, uint32_t color) {
    tty_t *tty = tty_get();
    
    // Clamp rectangle to screen bounds
    if (x >= tty->width || y >= tty->height) {
        return;
    }
    
    // Clamp width and height
    if (x + w > tty->width) {
        w = tty->width - x;
    }
    if (y + h > tty->height) {
        h = tty->height - y;
    }
    
    if (w == tty->width && x == 0) {
        uint64_t pixels_per_row = tty->pitch / 4;
        for (uint64_t dy = 0; dy < h; dy++) {
            uint64_t row_offset = (y + dy) * pixels_per_row;
            for (uint64_t dx = 0; dx < w; dx++) {
                tty->buffer[row_offset + dx] = color;
            }
        }
    } else {
        for (uint64_t dy = 0; dy < h; dy++) {
            for (uint64_t dx = 0; dx < w; dx++) {
                tty_putpixel(x + dx, y + dy, color);
            }
        }
    }
}