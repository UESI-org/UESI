#include <tty.h>

void tty_putchar(char c) {
    tty_t *tty = tty_get();
    if (!tty->buffer) return;
    
    if (tty->cursor_visible) {
        // Manually erase cursor at current position
        uint64_t px = tty->cursor_x * tty->char_width;
        uint64_t py = tty->cursor_y * tty->char_height;
        for (uint64_t x = 0; x < tty->char_width; x++) {
            tty_putpixel(px + x, py + tty->char_height - 1, tty->bg_color);
            tty_putpixel(px + x, py + tty->char_height - 2, tty->bg_color);
        }
    }
    
    switch (c) {
        case '\n':
            tty->cursor_x = 0;
            tty->cursor_y++;
            break;
        case '\r':
            tty->cursor_x = 0;
            break;
        case '\t': {
            uint64_t next_tab = ((tty->cursor_x / 8) + 1) * 8;
            if (next_tab >= tty->cols) {
                tty->cursor_x = 0;
                tty->cursor_y++;
            } else {
                tty->cursor_x = next_tab;
            }
            break;
        }
        case '\b':
            if (tty->cursor_x > 0) {
                tty->cursor_x--;
                uint64_t px = tty->cursor_x * tty->char_width;
                uint64_t py = tty->cursor_y * tty->char_height;
                tty_fill_rect(px, py, tty->char_width, tty->char_height, tty->bg_color);
            }
            break;
        default:
            if (c >= 32 && c <= 126) {
                uint64_t px = tty->cursor_x * tty->char_width;
                uint64_t py = tty->cursor_y * tty->char_height;
                // Bounds check before drawing
                if (px < tty->width && py < tty->height) {
                    tty_draw_char(c, px, py, tty->fg_color, tty->bg_color);
                }
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
    if (!str || len == 0) return;
    
    for (size_t i = 0; i < len && str[i] != '\0'; i++) {
        tty_putchar(str[i]);
    }
}

void tty_writestring(const char *str) {
    if (!str) return;
    
    while (*str) {
        tty_putchar(*str++);
    }
}