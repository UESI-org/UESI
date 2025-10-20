#include "tty.h"

void tty_clear(void) {
    tty_t *tty = tty_get();
    
    tty_fill_rect(0, 0, tty->width, tty->height, tty->bg_color);
    
    tty->cursor_x = 0;
    tty->cursor_y = 0;
}

void tty_scroll(void) {
    tty_t *tty = tty_get();
    
    uint64_t line_height = tty->char_height;
    
    for (uint64_t y = line_height; y < tty->height; y++) {
        for (uint64_t x = 0; x < tty->width; x++) {
            uint32_t pixel = tty_getpixel(x, y);
            tty_putpixel(x, y - line_height, pixel);
        }
    }
    
    uint64_t last_line_y = (tty->rows - 1) * tty->char_height;
    tty_fill_rect(0, last_line_y, tty->width, line_height, tty->bg_color);
}