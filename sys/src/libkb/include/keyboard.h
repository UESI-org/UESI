#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>
#include <printf.h>

#define KEY_ESCAPE      0x01
#define KEY_BACKSPACE   0x0E
#define KEY_TAB         0x0F
#define KEY_ENTER       0x1C
#define KEY_LCTRL       0x1D
#define KEY_LSHIFT      0x2A
#define KEY_RSHIFT      0x36
#define KEY_LALT        0x38
#define KEY_SPACE       0x39
#define KEY_CAPSLOCK    0x3A
#define KEY_F1          0x3B
#define KEY_F2          0x3C
#define KEY_F3          0x3D
#define KEY_F4          0x3E
#define KEY_F5          0x3F
#define KEY_F6          0x40
#define KEY_F7          0x41
#define KEY_F8          0x42
#define KEY_F9          0x43
#define KEY_F10         0x44
#define KEY_NUMLOCK     0x45
#define KEY_SCROLLLOCK  0x46
#define KEY_F11         0x57
#define KEY_F12         0x58

#define KEY_UP          0x48
#define KEY_DOWN        0x50
#define KEY_LEFT        0x4B
#define KEY_RIGHT       0x4D
#define KEY_HOME        0x47
#define KEY_END         0x4F
#define KEY_PAGEUP      0x49
#define KEY_PAGEDOWN    0x51
#define KEY_INSERT      0x52
#define KEY_DELETE      0x53

typedef struct {
    bool shift;
    bool ctrl;
    bool alt;
    bool capslock;
    bool numlock;
    bool scrolllock;
    bool extended;  // For handling 0xE0 prefix
} keyboard_state_t;

typedef void (*keyboard_callback_t)(char c);

void keyboard_init(void);

keyboard_state_t keyboard_get_state(void);

void keyboard_set_callback(keyboard_callback_t callback);

char keyboard_getchar(void);
bool keyboard_has_key(void);

#endif