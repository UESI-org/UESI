#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <io.h>

#define SERIAL_COM1 0x3F8
#define SERIAL_COM2 0x2F8
#define SERIAL_COM3 0x3E8
#define SERIAL_COM4 0x2E8

#define SERIAL_DATA_REG 0         // Data register (read/write)
#define SERIAL_INT_ENABLE_REG 1   // Interrupt Enable Register
#define SERIAL_INT_ID_REG 2       // Interrupt Identification Register (read)
#define SERIAL_FIFO_CTRL_REG 2    // FIFO Control Register (write)
#define SERIAL_LINE_CTRL_REG 3    // Line Control Register
#define SERIAL_MODEM_CTRL_REG 4   // Modem Control Register
#define SERIAL_LINE_STATUS_REG 5  // Line Status Register
#define SERIAL_MODEM_STATUS_REG 6 // Modem Status Register
#define SERIAL_SCRATCH_REG 7      // Scratch Register

#define SERIAL_DIVISOR_LOW 0  // Divisor Latch Low Byte
#define SERIAL_DIVISOR_HIGH 1 // Divisor Latch High Byte

#define SERIAL_LCR_DLAB 0x80 // Divisor Latch Access Bit

#define SERIAL_LSR_DATA_READY 0x01
#define SERIAL_LSR_OVERRUN_ERROR 0x02
#define SERIAL_LSR_PARITY_ERROR 0x04
#define SERIAL_LSR_FRAMING_ERROR 0x08
#define SERIAL_LSR_BREAK_INDICATOR 0x10
#define SERIAL_LSR_TRANSMIT_EMPTY 0x20
#define SERIAL_LSR_TRANSMITTER_IDLE 0x40
#define SERIAL_LSR_FIFO_ERROR 0x80

#define SERIAL_MCR_DTR 0x01  // Data Terminal Ready
#define SERIAL_MCR_RTS 0x02  // Request to Send
#define SERIAL_MCR_OUT1 0x04 // Out 1
#define SERIAL_MCR_OUT2 0x08 // Out 2 (enables IRQ in PC)
#define SERIAL_MCR_LOOP 0x10 // Loopback mode

#define SERIAL_IER_RX_AVAILABLE 0x01 // Received Data Available
#define SERIAL_IER_TX_EMPTY 0x02     // Transmitter Holding Register Empty
#define SERIAL_IER_LINE_STATUS 0x04  // Receiver Line Status
#define SERIAL_IER_MODEM_STATUS 0x08 // Modem Status

#define SERIAL_FCR_ENABLE 0x01     // Enable FIFOs
#define SERIAL_FCR_CLEAR_RX 0x02   // Clear Receive FIFO
#define SERIAL_FCR_CLEAR_TX 0x04   // Clear Transmit FIFO
#define SERIAL_FCR_DMA_MODE 0x08   // DMA Mode Select
#define SERIAL_FCR_TRIGGER_1 0x00  // 1 byte trigger
#define SERIAL_FCR_TRIGGER_4 0x40  // 4 byte trigger
#define SERIAL_FCR_TRIGGER_8 0x80  // 8 byte trigger
#define SERIAL_FCR_TRIGGER_14 0xC0 // 14 byte trigger

#define SERIAL_BAUD_115200 1
#define SERIAL_BAUD_57600 2
#define SERIAL_BAUD_38400 3
#define SERIAL_BAUD_19200 6
#define SERIAL_BAUD_9600 12
#define SERIAL_BAUD_4800 24
#define SERIAL_BAUD_2400 48

#define SERIAL_DATA_5BITS 0x00
#define SERIAL_DATA_6BITS 0x01
#define SERIAL_DATA_7BITS 0x02
#define SERIAL_DATA_8BITS 0x03

#define SERIAL_STOP_1BIT 0x00
#define SERIAL_STOP_2BITS 0x04

#define SERIAL_PARITY_NONE 0x00
#define SERIAL_PARITY_ODD 0x08
#define SERIAL_PARITY_EVEN 0x18
#define SERIAL_PARITY_MARK 0x28
#define SERIAL_PARITY_SPACE 0x38

bool serial_init(uint16_t port);

bool serial_init_custom(uint16_t port,
                        uint16_t divisor,
                        uint8_t data_bits,
                        uint8_t stop_bits,
                        uint8_t parity);

bool serial_is_transmit_ready(uint16_t port);
bool serial_is_data_available(uint16_t port);

void serial_write_byte(uint16_t port, uint8_t data);
uint8_t serial_read_byte(uint16_t port);

void serial_write_string(uint16_t port, const char *str);
void serial_write(uint16_t port, const uint8_t *buffer, size_t size);
size_t serial_read(uint16_t port, uint8_t *buffer, size_t size);

void serial_printf(uint16_t port, const char *fmt, ...);

#endif