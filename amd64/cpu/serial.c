#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <serial.h>
#include <io.h>

bool
serial_init(uint16_t port)
{
	return serial_init_custom(port,
	                          SERIAL_BAUD_115200,
	                          SERIAL_DATA_8BITS,
	                          SERIAL_STOP_1BIT,
	                          SERIAL_PARITY_NONE);
}

bool
serial_init_custom(uint16_t port,
                   uint16_t divisor,
                   uint8_t data_bits,
                   uint8_t stop_bits,
                   uint8_t parity)
{
	// Disable all interrupts
	outb(port + SERIAL_INT_ENABLE_REG, 0x00);

	// Enable DLAB (set baud rate divisor)
	outb(port + SERIAL_LINE_CTRL_REG, SERIAL_LCR_DLAB);

	// Set divisor (lo byte)
	outb(port + SERIAL_DIVISOR_LOW, divisor & 0xFF);

	// Set divisor (hi byte)
	outb(port + SERIAL_DIVISOR_HIGH, (divisor >> 8) & 0xFF);

	// Clear DLAB and set data format (data bits, stop bits, parity)
	uint8_t lcr = data_bits | stop_bits | parity;
	outb(port + SERIAL_LINE_CTRL_REG, lcr);

	// Enable FIFO, clear them, with 14-byte threshold
	outb(port + SERIAL_FIFO_CTRL_REG,
	     SERIAL_FCR_ENABLE | SERIAL_FCR_CLEAR_RX | SERIAL_FCR_CLEAR_TX |
	         SERIAL_FCR_TRIGGER_14);

	// IRQs enabled, RTS/DSR set
	outb(port + SERIAL_MODEM_CTRL_REG,
	     SERIAL_MCR_DTR | SERIAL_MCR_RTS | SERIAL_MCR_OUT2);

	// Set in loopback mode, test the serial chip
	outb(port + SERIAL_MODEM_CTRL_REG,
	     SERIAL_MCR_DTR | SERIAL_MCR_RTS | SERIAL_MCR_OUT1 |
	         SERIAL_MCR_OUT2 | SERIAL_MCR_LOOP);

	// Test serial chip (send byte 0xAE and check if serial returns same
	// byte)
	outb(port + SERIAL_DATA_REG, 0xAE);

	// Check if serial is faulty (i.e: not same byte as sent)
	if (inb(port + SERIAL_DATA_REG) != 0xAE) {
		return false;
	}

	outb(port + SERIAL_MODEM_CTRL_REG,
	     SERIAL_MCR_DTR | SERIAL_MCR_RTS | SERIAL_MCR_OUT1 |
	         SERIAL_MCR_OUT2);

	return true;
}

bool
serial_is_transmit_ready(uint16_t port)
{
	return (inb(port + SERIAL_LINE_STATUS_REG) &
	        SERIAL_LSR_TRANSMIT_EMPTY) != 0;
}

bool
serial_is_data_available(uint16_t port)
{
	return (inb(port + SERIAL_LINE_STATUS_REG) & SERIAL_LSR_DATA_READY) !=
	       0;
}

void
serial_write_byte(uint16_t port, uint8_t data)
{
	while (!serial_is_transmit_ready(port))
		;
	outb(port + SERIAL_DATA_REG, data);
}

uint8_t
serial_read_byte(uint16_t port)
{
	while (!serial_is_data_available(port))
		;
	return inb(port + SERIAL_DATA_REG);
}

void
serial_write_string(uint16_t port, const char *str)
{
	if (!str)
		return;
	while (*str) {
		serial_write_byte(port, *str++);
	}
}

void
serial_write(uint16_t port, const uint8_t *buffer, size_t size)
{
	if (!buffer)
		return;
	for (size_t i = 0; i < size; i++) {
		serial_write_byte(port, buffer[i]);
	}
}

size_t
serial_read(uint16_t port, uint8_t *buffer, size_t size)
{
	if (!buffer || size == 0)
		return 0;
	size_t bytes_read = 0;
	while (bytes_read < size && serial_is_data_available(port)) {
		buffer[bytes_read++] = serial_read_byte(port);
	}
	return bytes_read;
}

static void
print_num(uint16_t port,
          unsigned long long num,
          int base,
          int width,
          char pad,
          bool uppercase)
{
	char buf[32];
	int i = 0;

	if (num == 0) {
		buf[i++] = '0';
	} else {
		while (num > 0) {
			int digit = num % base;
			if (digit < 10) {
				buf[i++] = '0' + digit;
			} else {
				buf[i++] =
				    (uppercase ? 'A' : 'a') + (digit - 10);
			}
			num /= base;
		}
	}

	while (i < width) {
		serial_write_byte(port, pad);
		width--;
	}

	while (i-- > 0) {
		serial_write_byte(port, buf[i]);
	}
}

static void
print_signed(uint16_t port, long long num, int width, char pad)
{
	if (num < 0) {
		serial_write_byte(port, '-');
		num = -num;
		if (width > 0)
			width--;
	}
	print_num(port, num, 10, width, pad, false);
}

void
serial_printf(uint16_t port, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	while (*fmt) {
		if (*fmt == '%' && *(fmt + 1)) {
			fmt++;

			bool left_align = false;
			char pad = ' ';
			int width = 0;

			if (*fmt == '-') {
				left_align = true;
				fmt++;
			}

			if (*fmt == '0') {
				pad = '0';
				fmt++;
			}

			while (*fmt >= '0' && *fmt <= '9') {
				width = width * 10 + (*fmt - '0');
				fmt++;
			}

			bool is_long_long = false;
			if (*fmt == 'l' && *(fmt + 1) == 'l') {
				is_long_long = true;
				fmt += 2;
			} else if (*fmt == 'l') {
				fmt++;
			}

			switch (*fmt) {
			case 'd':
			case 'i':
				if (is_long_long) {
					print_signed(port,
					             va_arg(args, long long),
					             width,
					             pad);
				} else {
					print_signed(port,
					             va_arg(args, int),
					             width,
					             pad);
				}
				break;
			case 'u':
				if (is_long_long) {
					print_num(
					    port,
					    va_arg(args, unsigned long long),
					    10,
					    width,
					    pad,
					    false);
				} else {
					print_num(port,
					          va_arg(args, unsigned int),
					          10,
					          width,
					          pad,
					          false);
				}
				break;
			case 'x':
				if (is_long_long) {
					print_num(
					    port,
					    va_arg(args, unsigned long long),
					    16,
					    width,
					    pad,
					    false);
				} else {
					print_num(port,
					          va_arg(args, unsigned int),
					          16,
					          width,
					          pad,
					          false);
				}
				break;
			case 'X':
				if (is_long_long) {
					print_num(
					    port,
					    va_arg(args, unsigned long long),
					    16,
					    width,
					    pad,
					    true);
				} else {
					print_num(port,
					          va_arg(args, unsigned int),
					          16,
					          width,
					          pad,
					          true);
				}
				break;
			case 'p':
				serial_write_string(port, "0x");
				print_num(
				    port,
				    (unsigned long long)va_arg(args, void *),
				    16,
				    width,
				    pad,
				    false);
				break;
			case 's': {
				char *str = va_arg(args, char *);
				if (!str) {
					str = "(null)";
				}

				int len = strlen(str);
				int padding = width > len ? width - len : 0;

				// Left-aligned: print string first, then
				// padding
				if (left_align) {
					serial_write_string(port, str);
					for (int i = 0; i < padding; i++) {
						serial_write_byte(port, ' ');
					}
				}
				// Right-aligned: print padding first, then
				// string
				else {
					for (int i = 0; i < padding; i++) {
						serial_write_byte(port, ' ');
					}
					serial_write_string(port, str);
				}
				break;
			}
			case 'c':
				serial_write_byte(port,
				                  (char)va_arg(args, int));
				break;
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