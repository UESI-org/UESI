#include "tty.h"
#include <stdarg.h>

static void itoa(int64_t value, char *str, int base) {
    char *ptr = str;
    char *ptr1 = str;
    char tmp_char;
    int64_t tmp_value;
    
    if (value == 0) {
        *ptr++ = '0';
        *ptr = '\0';
        return;
    }
    
    bool negative = false;
    if (value < 0 && base == 10) {
        negative = true;
        value = -value;
    }
    
    while (value) {
        tmp_value = value;
        value /= base;
        *ptr++ = "0123456789abcdef"[tmp_value - value * base];
    }
    
    if (negative) {
        *ptr++ = '-';
    }
    
    *ptr-- = '\0';
    
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
}

static void uitoa(uint64_t value, char *str, int base) {
    char *ptr = str;
    char *ptr1 = str;
    char tmp_char;
    uint64_t tmp_value;
    
    if (value == 0) {
        *ptr++ = '0';
        *ptr = '\0';
        return;
    }
    
    while (value) {
        tmp_value = value;
        value /= base;
        *ptr++ = "0123456789abcdef"[tmp_value - value * base];
    }
    
    *ptr-- = '\0';
    
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
}

void tty_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    char buffer[32];
    
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
                case 'd':
                case 'i': {
                    int value = va_arg(args, int);
                    itoa(value, buffer, 10);
                    tty_writestring(buffer);
                    break;
                }
                case 'u': {
                    unsigned int value = va_arg(args, unsigned int);
                    uitoa(value, buffer, 10);
                    tty_writestring(buffer);
                    break;
                }
                case 'x': {
                    unsigned int value = va_arg(args, unsigned int);
                    uitoa(value, buffer, 16);
                    tty_writestring(buffer);
                    break;
                }
                case 'X': {
                    unsigned int value = va_arg(args, unsigned int);
                    uitoa(value, buffer, 16);
                    for (char *p = buffer; *p; p++) {
                        if (*p >= 'a' && *p <= 'f')
                            *p = *p - 'a' + 'A';
                    }
                    tty_writestring(buffer);
                    break;
                }
                case 'p': {
                    void *ptr = va_arg(args, void*);
                    tty_writestring("0x");
                    uitoa((uint64_t)ptr, buffer, 16);
                    tty_writestring(buffer);
                    break;
                }
                case 's': {
                    char *str = va_arg(args, char*);
                    if (str) {
                        tty_writestring(str);
                    } else {
                        tty_writestring("(null)");
                    }
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(args, int);
                    tty_putchar(c);
                    break;
                }
                case '%': {
                    tty_putchar('%');
                    break;
                }
                default:
                    tty_putchar('%');
                    tty_putchar(*fmt);
                    break;
            }
        } else {
            tty_putchar(*fmt);
        }
        fmt++;
    }
    
    va_end(args);
}