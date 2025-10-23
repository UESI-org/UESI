#include "tty.h"

// ANSI 256-color palette
static uint32_t ansi_palette[256] = {0};
static bool palette_initialized = false;

static void tty_init_palette(void) {
    if (palette_initialized) return;
    
    ansi_palette[0]  = TTY_COLOR_BLACK;
    ansi_palette[1]  = TTY_COLOR_RED;
    ansi_palette[2]  = TTY_COLOR_GREEN;
    ansi_palette[3]  = TTY_COLOR_BROWN;
    ansi_palette[4]  = TTY_COLOR_BLUE;
    ansi_palette[5]  = TTY_COLOR_MAGENTA;
    ansi_palette[6]  = TTY_COLOR_CYAN;
    ansi_palette[7]  = TTY_COLOR_LIGHT_GRAY;
    ansi_palette[8]  = TTY_COLOR_DARK_GRAY;
    ansi_palette[9]  = TTY_COLOR_LIGHT_RED;
    ansi_palette[10] = TTY_COLOR_LIGHT_GREEN;
    ansi_palette[11] = TTY_COLOR_YELLOW;
    ansi_palette[12] = TTY_COLOR_LIGHT_BLUE;
    ansi_palette[13] = TTY_COLOR_LIGHT_MAGENTA;
    ansi_palette[14] = TTY_COLOR_LIGHT_CYAN;
    ansi_palette[15] = TTY_COLOR_WHITE;
    
    for (int i = 0; i < 216; i++) {
        int r = (i / 36) % 6;
        int g = (i / 6) % 6;
        int b = i % 6;
        
        uint32_t rv = (r == 0) ? 0 : (r * 40 + 55);
        uint32_t gv = (g == 0) ? 0 : (g * 40 + 55);
        uint32_t bv = (b == 0) ? 0 : (b * 40 + 55);
        
        ansi_palette[16 + i] = (rv << 16) | (gv << 8) | bv;
    }
    
    for (int i = 0; i < 24; i++) {
        uint32_t gray = 8 + i * 10;
        ansi_palette[232 + i] = (gray << 16) | (gray << 8) | gray;
    }
    
    palette_initialized = true;
}

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

void tty_set_color_256(uint8_t fg, uint8_t bg) {
    if (!palette_initialized) {
        tty_init_palette();
    }
    
    tty_t *tty = tty_get();
    tty->fg_color = ansi_palette[fg];
    tty->bg_color = ansi_palette[bg];
}

uint32_t tty_get_palette_color(uint8_t index) {
    if (!palette_initialized) {
        tty_init_palette();
    }
    
    return ansi_palette[index];
}

void tty_set_palette_color(uint8_t index, uint32_t rgb) {
    if (!palette_initialized) {
        tty_init_palette();
    }
    
    ansi_palette[index] = rgb;
}

uint8_t tty_rgb_to_palette(uint8_t r, uint8_t g, uint8_t b) {
    if (!palette_initialized) {
        tty_init_palette();
    }
    
    int ri = (r * 6) / 256;
    int gi = (g * 6) / 256;
    int bi = (b * 6) / 256;
    
    if (ri > 5) ri = 5;
    if (gi > 5) gi = 5;
    if (bi > 5) bi = 5;
    
    return 16 + (ri * 36) + (gi * 6) + bi;
}