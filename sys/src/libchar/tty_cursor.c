#include "tty.h"

void tty_set_cursor(uint64_t x, uint64_t y) {
    tty_t *tty = tty_get();
    
    if (x < tty->cols && y < tty->rows) {
        tty->cursor_x = x;
        tty->cursor_y = y;
    }
}

void tty_get_cursor(uint64_t *x, uint64_t *y) {
    tty_t *tty = tty_get();
    
    if (x) *x = tty->cursor_x;
    if (y) *y = tty->cursor_y;
}

void tty_show_cursor(bool visible) {
    tty_t *tty = tty_get();
    tty->cursor_visible = visible;
}

void tty_draw_cursor(void) {
    tty_t *tty = tty_get();
    
    static uint64_t last_cursor_x = 0;
    static uint64_t last_cursor_y = 0;
    static bool first_draw = true;
    
    if (!first_draw) {
        uint64_t old_px = last_cursor_x * tty->char_width;
        uint64_t old_py = last_cursor_y * tty->char_height;
        
        for (uint64_t x = 0; x < tty->char_width; x++) {
            tty_putpixel(old_px + x, old_py + tty->char_height - 1, tty->bg_color);
        }
    }
    
    if (tty->cursor_visible) {
        uint64_t px = tty->cursor_x * tty->char_width;
        uint64_t py = tty->cursor_y * tty->char_height;
        
        for (uint64_t x = 0; x < tty->char_width; x++) {
            tty_putpixel(px + x, py + tty->char_height - 1, tty->fg_color);
        }
    }
    
    last_cursor_x = tty->cursor_x;
    last_cursor_y = tty->cursor_y;
    first_draw = false;
}