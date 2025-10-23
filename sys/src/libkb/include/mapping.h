#ifndef MAPPING_H
#define MAPPING_H

#include <stdint.h>

// QWERTZ keyboard layout mapping

static const char scancode_to_ascii[] = {
    0,    // 0x00
    0,    // 0x01 - ESC
    '1',  // 0x02
    '2',  // 0x03
    '3',  // 0x04
    '4',  // 0x05
    '5',  // 0x06
    '6',  // 0x07
    '7',  // 0x08
    '8',  // 0x09
    '9',  // 0x0A
    '0',  // 0x0B
    0xE1, // 0x0C - ß (German sharp s)
    '\'', // 0x0D - ´ (acute accent, using apostrophe)
    '\b', // 0x0E - Backspace
    '\t', // 0x0F - Tab
    'q',  // 0x10
    'w',  // 0x11
    'e',  // 0x12
    'r',  // 0x13
    't',  // 0x14
    'z',  // 0x15 - Z in QWERTZ
    'u',  // 0x16
    'i',  // 0x17
    'o',  // 0x18
    'p',  // 0x19
    0xFC, // 0x1A - ü
    '+',  // 0x1B
    '\n', // 0x1C - Enter
    0,    // 0x1D - Left Control
    'a',  // 0x1E
    's',  // 0x1F
    'd',  // 0x20
    'f',  // 0x21
    'g',  // 0x22
    'h',  // 0x23
    'j',  // 0x24
    'k',  // 0x25
    'l',  // 0x26
    0xF6, // 0x27 - ö
    0xE4, // 0x28 - ä
    '^',  // 0x29 - ^ (caret)
    0,    // 0x2A - Left Shift
    '#',  // 0x2B
    'y',  // 0x2C - Y in QWERTZ
    'x',  // 0x2D
    'c',  // 0x2E
    'v',  // 0x2F
    'b',  // 0x30
    'n',  // 0x31
    'm',  // 0x32
    ',',  // 0x33
    '.',  // 0x34
    '-',  // 0x35
    0,    // 0x36 - Right Shift
    '*',  // 0x37 - Keypad *
    0,    // 0x38 - Left Alt
    ' ',  // 0x39 - Space
    0,    // 0x3A - Caps Lock
    0,    // 0x3B - F1
    0,    // 0x3C - F2
    0,    // 0x3D - F3
    0,    // 0x3E - F4
    0,    // 0x3F - F5
    0,    // 0x40 - F6
    0,    // 0x41 - F7
    0,    // 0x42 - F8
    0,    // 0x43 - F9
    0,    // 0x44 - F10
    0,    // 0x45 - Num Lock
    0,    // 0x46 - Scroll Lock
    '7',  // 0x47 - Keypad 7 / Home
    '8',  // 0x48 - Keypad 8 / Up
    '9',  // 0x49 - Keypad 9 / Page Up
    '-',  // 0x4A - Keypad -
    '4',  // 0x4B - Keypad 4 / Left
    '5',  // 0x4C - Keypad 5
    '6',  // 0x4D - Keypad 6 / Right
    '+',  // 0x4E - Keypad +
    '1',  // 0x4F - Keypad 1 / End
    '2',  // 0x50 - Keypad 2 / Down
    '3',  // 0x51 - Keypad 3 / Page Down
    '0',  // 0x52 - Keypad 0 / Insert
    '.',  // 0x53 - Keypad . / Delete
    0,    // 0x54
    0,    // 0x55
    '<',  // 0x56 - < > key (between left shift and Z)
    0,    // 0x57 - F11
    0,    // 0x58 - F12
};

// Shift key map (with shift pressed)
static const char scancode_to_ascii_shift[] = {
    0,    // 0x00
    0,    // 0x01 - ESC
    '!',  // 0x02
    '"',  // 0x03
    0xA7, // 0x04 - § (section sign)
    '$',  // 0x05
    '%',  // 0x06
    '&',  // 0x07
    '/',  // 0x08
    '(',  // 0x09
    ')',  // 0x0A
    '=',  // 0x0B
    '?',  // 0x0C - ? (shifted ß)
    '`',  // 0x0D - ` (grave accent)
    '\b', // 0x0E - Backspace
    '\t', // 0x0F - Tab
    'Q',  // 0x10
    'W',  // 0x11
    'E',  // 0x12
    'R',  // 0x13
    'T',  // 0x14
    'Z',  // 0x15 - Z in QWERTZ
    'U',  // 0x16
    'I',  // 0x17
    'O',  // 0x18
    'P',  // 0x19
    0xDC, // 0x1A - Ü
    '*',  // 0x1B
    '\n', // 0x1C - Enter
    0,    // 0x1D - Left Control
    'A',  // 0x1E
    'S',  // 0x1F
    'D',  // 0x20
    'F',  // 0x21
    'G',  // 0x22
    'H',  // 0x23
    'J',  // 0x24
    'K',  // 0x25
    'L',  // 0x26
    0xD6, // 0x27 - Ö
    0xC4, // 0x28 - Ä
    0xB0, // 0x29 - ° (degree sign)
    0,    // 0x2A - Left Shift
    '\'', // 0x2B
    'Y',  // 0x2C - Y in QWERTZ
    'X',  // 0x2D
    'C',  // 0x2E
    'V',  // 0x2F
    'B',  // 0x30
    'N',  // 0x31
    'M',  // 0x32
    ';',  // 0x33
    ':',  // 0x34
    '_',  // 0x35
    0,    // 0x36 - Right Shift
    '*',  // 0x37 - Keypad *
    0,    // 0x38 - Left Alt
    ' ',  // 0x39 - Space
    0,    // 0x3A - Caps Lock
    0,    // 0x3B - F1
    0,    // 0x3C - F2
    0,    // 0x3D - F3
    0,    // 0x3E - F4
    0,    // 0x3F - F5
    0,    // 0x40 - F6
    0,    // 0x41 - F7
    0,    // 0x42 - F8
    0,    // 0x43 - F9
    0,    // 0x44 - F10
    0,    // 0x45 - Num Lock
    0,    // 0x46 - Scroll Lock
    '7',  // 0x47 - Keypad 7 / Home
    '8',  // 0x48 - Keypad 8 / Up
    '9',  // 0x49 - Keypad 9 / Page Up
    '-',  // 0x4A - Keypad -
    '4',  // 0x4B - Keypad 4 / Left
    '5',  // 0x4C - Keypad 5
    '6',  // 0x4D - Keypad 6 / Right
    '+',  // 0x4E - Keypad +
    '1',  // 0x4F - Keypad 1 / End
    '2',  // 0x50 - Keypad 2 / Down
    '3',  // 0x51 - Keypad 3 / Page Down
    '0',  // 0x52 - Keypad 0 / Insert
    '.',  // 0x53 - Keypad . / Delete
    0,    // 0x54
    0,    // 0x55
    '>',  // 0x56 - < > key
    0,    // 0x57 - F11
    0,    // 0x58 - F12
};

#endif