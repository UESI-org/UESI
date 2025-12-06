#include <tty.h>
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

static void pad_hex(char *buffer, int width) {
    int len = 0;
    while (buffer[len]) len++;
    
    if (len >= width) return;
    
    for (int i = len; i >= 0; i--) {
        buffer[i + (width - len)] = buffer[i];
    }
    
    for (int i = 0; i < width - len; i++) {
        buffer[i] = '0';
    }
}

void tty_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    char buffer[32];
    
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            
            bool is_long_long = false;
            bool is_long = false;
            
            if (*fmt == 'l') {
                if (*(fmt + 1) == 'l') {
                    is_long_long = true;
                    fmt += 2;
                } else {
                    is_long = true;
                    fmt++;
                }
            }
            
            bool zero_pad = false;
            int width = 0;
            if (*fmt == '0') {
                zero_pad = true;
                fmt++;
                while (*fmt >= '0' && *fmt <= '9') {
                    width = width * 10 + (*fmt - '0');
                    fmt++;
                }
            }
            
            switch (*fmt) {
                case 'd':
                case 'i': {
                    if (is_long_long) {
                        int64_t value = va_arg(args, int64_t);
                        itoa(value, buffer, 10);
                    } else if (is_long) {
                        long value = va_arg(args, long);
                        itoa(value, buffer, 10);
                    } else {
                        int value = va_arg(args, int);
                        itoa(value, buffer, 10);
                    }
                    tty_writestring(buffer);
                    break;
                }
                case 'u': {
                    if (is_long_long) {
                        uint64_t value = va_arg(args, uint64_t);
                        uitoa(value, buffer, 10);
                    } else if (is_long) {
                        unsigned long value = va_arg(args, unsigned long);
                        uitoa(value, buffer, 10);
                    } else {
                        unsigned int value = va_arg(args, unsigned int);
                        uitoa(value, buffer, 10);
                    }
                    tty_writestring(buffer);
                    break;
                }
                case 'x': {
                    if (is_long_long) {
                        uint64_t value = va_arg(args, uint64_t);
                        uitoa(value, buffer, 16);
                    } else if (is_long) {
                        unsigned long value = va_arg(args, unsigned long);
                        uitoa(value, buffer, 16);
                    } else {
                        unsigned int value = va_arg(args, unsigned int);
                        uitoa(value, buffer, 16);
                    }
                    if (zero_pad && width > 0) {
                        pad_hex(buffer, width);
                    }
                    tty_writestring(buffer);
                    break;
                }
                case 'X': {
                    if (is_long_long) {
                        uint64_t value = va_arg(args, uint64_t);
                        uitoa(value, buffer, 16);
                    } else if (is_long) {
                        unsigned long value = va_arg(args, unsigned long);
                        uitoa(value, buffer, 16);
                    } else {
                        unsigned int value = va_arg(args, unsigned int);
                        uitoa(value, buffer, 16);
                    }
                    if (zero_pad && width > 0) {
                        pad_hex(buffer, width);
                    }
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
                    pad_hex(buffer, 16);  // Always pad pointers to 16 hex digits
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
                    if (is_long_long) {
                        tty_writestring("ll");
                    } else if (is_long) {
                        tty_putchar('l');
                    }
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