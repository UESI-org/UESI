#include "tty.h"

void tty_putchar(char c) {
    tty_t *tty = tty_get();
    
    switch (c) {
        case '\n':
            tty->cursor_x = 0;
            tty->cursor_y++;
            break;
            
        case '\r':
            tty->cursor_x = 0;
            break;
            
        case '\t':
            tty->cursor_x = (tty->cursor_x + 8) & ~7;
            break;
            
        case '\b':
            if (tty->cursor_x > 0) {
                tty->cursor_x--;
            }
            break;
            
        default:
            if (c >= 32 && c <= 126) {
                uint64_t px = tty->cursor_x * tty->char_width;
                uint64_t py = tty->cursor_y * tty->char_height;
                
                tty_draw_char(c, px, py, tty->fg_color, tty->bg_color);
                tty->cursor_x++;
            }
            break;
    }
    
    if (tty->cursor_x >= tty->cols) {
        tty->cursor_x = 0;
        tty->cursor_y++;
    }
    
    if (tty->cursor_y >= tty->rows) {
        tty_scroll();
        tty->cursor_y = tty->rows - 1;
    }
    
    if (tty->cursor_visible) {
        tty_draw_cursor();
    }
}

void tty_write(const char *str, size_t len) {
    for (size_t i = 0; i < len; i++) {
        tty_putchar(str[i]);
    }
}

void tty_writestring(const char *str) {
    while (*str) {
        tty_putchar(*str++);
    }
}