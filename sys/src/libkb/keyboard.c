#include "include/keyboard.h"
#include "include/mapping.h"
#include "isr.h"
#include <stddef.h>

extern void tty_putchar(char c);
extern void tty_printf(const char *fmt, ...);

#define KEYBOARD_DATA_PORT    0x60
#define KEYBOARD_STATUS_PORT  0x64
#define KEYBOARD_COMMAND_PORT 0x64

#define KEYBOARD_BUFFER_SIZE 256
static char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static volatile uint32_t buffer_read_pos = 0;
static volatile uint32_t buffer_write_pos = 0;

static keyboard_state_t kbd_state = {
    .shift = false,
    .ctrl = false,
    .alt = false,
    .capslock = false,
    .numlock = false,
    .scrolllock = false,
    .extended = false
};

static keyboard_callback_t key_callback = NULL;

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void buffer_push(char c) {
    uint32_t next_pos = (buffer_write_pos + 1) % KEYBOARD_BUFFER_SIZE;
    if (next_pos != buffer_read_pos) {
        keyboard_buffer[buffer_write_pos] = c;
        buffer_write_pos = next_pos;
    }
}

static char buffer_pop(void) {
    if (buffer_read_pos == buffer_write_pos) {
        return 0;
    }
    char c = keyboard_buffer[buffer_read_pos];
    buffer_read_pos = (buffer_read_pos + 1) % KEYBOARD_BUFFER_SIZE;
    return c;
}

static bool buffer_has_data(void) {
    return buffer_read_pos != buffer_write_pos;
}

static char translate_scancode(uint8_t scancode) {
    if (kbd_state.extended) {
        kbd_state.extended = false;
        return 0;
    }
    
    if (scancode >= sizeof(scancode_to_ascii)) {
        return 0;
    }
    
    // Get base character
    char c;
    if (kbd_state.shift) {
        c = scancode_to_ascii_shift[scancode];
    } else {
        c = scancode_to_ascii[scancode];
    }
    
    if (kbd_state.capslock && c >= 'a' && c <= 'z') {
        c = c - 'a' + 'A';
    } else if (kbd_state.capslock && c >= 'A' && c <= 'Z') {
        c = c - 'A' + 'a';
    }
    
    return c;
}

static void keyboard_irq_handler(registers_t *regs) {
    (void)regs;
    
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);
    
    if (scancode == 0xE0) {
        kbd_state.extended = true;
        return;
    }
    
    bool released = (scancode & 0x80) != 0;
    scancode &= 0x7F; // Remove release bit
    
    if (scancode == KEY_LSHIFT || scancode == KEY_RSHIFT) {
        kbd_state.shift = !released;
        return;
    }
    
    if (scancode == KEY_LCTRL) {
        kbd_state.ctrl = !released;
        return;
    }
    
    if (scancode == KEY_LALT) {
        kbd_state.alt = !released;
        return;
    }
    
    // Handle lock keys (toggle on press only)
    if (!released) {
        if (scancode == KEY_CAPSLOCK) {
            kbd_state.capslock = !kbd_state.capslock;
            return;
        }
        
        if (scancode == KEY_NUMLOCK) {
            kbd_state.numlock = !kbd_state.numlock;
            return;
        }
        
        if (scancode == KEY_SCROLLLOCK) {
            kbd_state.scrolllock = !kbd_state.scrolllock;
            return;
        }
    }
    
    if (released) {
        return;
    }
    
    char c = translate_scancode(scancode);
    
    if (c != 0) {
        // Handle Ctrl combinations
        if (kbd_state.ctrl) {
            if (c >= 'a' && c <= 'z') {
                c = c - 'a' + 1;
            } else if (c >= 'A' && c <= 'Z') {
                c = c - 'A' + 1;
            }
        }
        
        buffer_push(c);
        
        if (key_callback != NULL) {
            key_callback(c);
        }
    }
}

void keyboard_init(void) {
    // Reset state
    kbd_state.shift = false;
    kbd_state.ctrl = false;
    kbd_state.alt = false;
    kbd_state.capslock = false;
    kbd_state.numlock = false;
    kbd_state.scrolllock = false;
    kbd_state.extended = false;
    
    buffer_read_pos = 0;
    buffer_write_pos = 0;
    
    isr_register_handler(33, keyboard_irq_handler);
    
    tty_printf("Keyboard driver initialized (QWERTZ layout)\n");
}

keyboard_state_t keyboard_get_state(void) {
    return kbd_state;
}

void keyboard_set_callback(keyboard_callback_t callback) {
    key_callback = callback;
}

char keyboard_getchar(void) {
    return buffer_pop();
}

bool keyboard_has_key(void) {
    return buffer_has_data();
}