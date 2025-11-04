#include "tty.h"

static tty_t tty;

void tty_init(struct limine_framebuffer *framebuffer) {
    tty.fb = framebuffer;
    tty.buffer = (uint32_t *)framebuffer->address;
    tty.width = framebuffer->width;
    tty.height = framebuffer->height;
    tty.pitch = framebuffer->pitch;
    tty.bpp = framebuffer->bpp;
    
    tty.char_width = 8;
    tty.char_height = 18;
    tty.cols = tty.width / tty.char_width;
    tty.rows = tty.height / tty.char_height;
    
    tty.cursor_x = 0;
    tty.cursor_y = 0;
    tty.cursor_visible = true;
    
    tty.fg_color = TTY_COLOR_WHITE;
    tty.bg_color = TTY_COLOR_BLACK;
    
    tty_clear();
}

tty_t *tty_get(void) {
    return &tty;
}
