#include "tty.h"

void tty_set_color(uint32_t fg, uint32_t bg) {
    tty_t *tty = tty_get();
    tty->fg_color = fg;
    tty->bg_color = bg;
}

void tty_get_color(uint32_t *fg, uint32_t *bg) {
    tty_t *tty = tty_get();
    
    if (fg) *fg = tty->fg_color;
    if (bg) *bg = tty->bg_color;
}