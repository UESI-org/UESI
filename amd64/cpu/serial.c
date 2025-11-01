#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>
#include "serial.h"
#include "io.h"

bool serial_init(uint16_t port) {
    // Default: 115200 baud, 8 data bits, 1 stop bit, no parity
    return serial_init_custom(port, SERIAL_BAUD_115200, 
                             SERIAL_DATA_8BITS, 
                             SERIAL_STOP_1BIT, 
                             SERIAL_PARITY_NONE);
}

bool serial_init_custom(uint16_t port, uint16_t divisor, uint8_t data_bits,
                        uint8_t stop_bits, uint8_t parity) {
    outb(port + SERIAL_INT_ENABLE_REG, 0x00);
    outb(port + SERIAL_LINE_CTRL_REG, 0x80);
    
    outb(port + SERIAL_DIVISOR_LOW, divisor & 0xFF);
    outb(port + SERIAL_DIVISOR_HIGH, (divisor >> 8) & 0xFF);
    
    uint8_t lcr = data_bits | stop_bits | parity;
    outb(port + SERIAL_LINE_CTRL_REG, lcr);
    
    outb(port + SERIAL_FIFO_CTRL_REG, 0xC7);
    
    // Enable IRQs, set RTS/DSR
    outb(port + SERIAL_MODEM_CTRL_REG, 0x0B);
    
    outb(port + SERIAL_MODEM_CTRL_REG, 0x1E);
    

    outb(port + SERIAL_DATA_REG, 0xAE);
    
    if (inb(port + SERIAL_DATA_REG) != 0xAE) {
        return false;
    }
    
    outb(port + SERIAL_MODEM_CTRL_REG, 0x0F);
    
    return true;
}

bool serial_is_transmit_ready(uint16_t port) {
    return (inb(port + SERIAL_LINE_STATUS_REG) & SERIAL_LSR_TRANSMIT_EMPTY) != 0;
}

bool serial_is_data_available(uint16_t port) {
    return (inb(port + SERIAL_LINE_STATUS_REG) & SERIAL_LSR_DATA_READY) != 0;
}

void serial_write_byte(uint16_t port, uint8_t data) {
    while (!serial_is_transmit_ready(port));
    
    outb(port + SERIAL_DATA_REG, data);
}

uint8_t serial_read_byte(uint16_t port) {
    while (!serial_is_data_available(port));
    
    return inb(port + SERIAL_DATA_REG);
}

void serial_write_string(uint16_t port, const char *str) {
    if (!str) return;
    
    while (*str) {
        serial_write_byte(port, *str++);
    }
}

void serial_write(uint16_t port, const uint8_t *buffer, size_t size) {
    if (!buffer) return;
    
    for (size_t i = 0; i < size; i++) {
        serial_write_byte(port, buffer[i]);
    }
}

size_t serial_read(uint16_t port, uint8_t *buffer, size_t size) {
    if (!buffer || size == 0) return 0;
    
    size_t bytes_read = 0;
    
    while (bytes_read < size && serial_is_data_available(port)) {
        buffer[bytes_read++] = serial_read_byte(port);
    }
    
    return bytes_read;
}

static void print_num(uint16_t port, unsigned long long num, int base) {
    char buf[32];
    int i = 0;
    
    if (num == 0) {
        serial_write_byte(port, '0');
        return;
    }
    
    while (num > 0) {
        int digit = num % base;
        buf[i++] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
        num /= base;
    }
    
    while (i > 0) {
        serial_write_byte(port, buf[--i]);
    }
}

static void print_signed(uint16_t port, long long num) {
    if (num < 0) {
        serial_write_byte(port, '-');
        num = -num;
    }
    print_num(port, num, 10);
}

void serial_printf(uint16_t port, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    while (*fmt) {
        if (*fmt == '%' && *(fmt + 1)) {
            fmt++;
            
            bool is_long_long = false;
            if (*fmt == 'l' && *(fmt + 1) == 'l') {
                is_long_long = true;
                fmt += 2;
            } else if (*fmt == 'l') {
                fmt++;
            }
            
            switch (*fmt) {
                case 'd':
                case 'i': {
                    if (is_long_long) {
                        long long val = va_arg(args, long long);
                        if (val < 0) {
                            serial_write_byte(port, '-');
                            val = -val;
                        }
                        print_num(port, (unsigned long long)val, 10);
                    } else {
                        int val = va_arg(args, int);
                        print_signed(port, val);
                    }
                    break;
                }
                case 'u': {
                    if (is_long_long) {
                        unsigned long long val = va_arg(args, unsigned long long);
                        print_num(port, val, 10);
                    } else {
                        unsigned int val = va_arg(args, unsigned int);
                        print_num(port, val, 10);
                    }
                    break;
                }
                case 'x': {
                    if (is_long_long) {
                        unsigned long long val = va_arg(args, unsigned long long);
                        print_num(port, val, 16);
                    } else {
                        unsigned int val = va_arg(args, unsigned int);
                        print_num(port, val, 16);
                    }
                    break;
                }
                case 'p': {
                    void *ptr = va_arg(args, void*);
                    serial_write_string(port, "0x");
                    print_num(port, (unsigned long long)ptr, 16);
                    break;
                }
                case 's': {
                    char *str = va_arg(args, char*);
                    serial_write_string(port, str ? str : "(null)");
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(args, int);
                    serial_write_byte(port, c);
                    break;
                }
                case '%':
                    serial_write_byte(port, '%');
                    break;
                default:
                    serial_write_byte(port, '%');
                    serial_write_byte(port, *fmt);
                    break;
            }
        } else {
            serial_write_byte(port, *fmt);
        }
        fmt++;
    }
    
    va_end(args);
}