#ifndef PIC_H
#define PIC_H

#include <stdint.h>

#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1

#define PIC_EOI 0x20
#define PIC_READ_IRR 0x0A
#define PIC_READ_ISR 0x0B

#define ICW1_ICW4 0x01      // ICW4 needed
#define ICW1_SINGLE 0x02    // Single (cascade) mode
#define ICW1_INTERVAL4 0x04 // Call address interval 4 (8)
#define ICW1_LEVEL 0x08     // Level triggered (edge) mode
#define ICW1_INIT 0x10      // Initialization

#define ICW4_8086 0x01       // 8086/88 (MCS-80/85) mode
#define ICW4_AUTO 0x02       // Auto (normal) EOI
#define ICW4_BUF_SLAVE 0x08  // Buffered mode/slave
#define ICW4_BUF_MASTER 0x0C // Buffered mode/master
#define ICW4_SFNM 0x10       // Special fully nested (not)

#define PIC1_OFFSET 0x20 // Master PIC: IRQs 0-7 -> INT 32-39
#define PIC2_OFFSET 0x28 // Slave PIC: IRQs 8-15 -> INT 40-47

#define IRQ_TIMER 0
#define IRQ_KEYBOARD 1
#define IRQ_CASCADE 2 // Cascade for slave PIC
#define IRQ_COM2 3
#define IRQ_COM1 4
#define IRQ_LPT2 5
#define IRQ_FLOPPY 6
#define IRQ_LPT1 7
#define IRQ_RTC 8
#define IRQ_ACPI 9
#define IRQ_AVAILABLE1 10
#define IRQ_AVAILABLE2 11
#define IRQ_MOUSE 12
#define IRQ_FPU 13
#define IRQ_ATA1 14
#define IRQ_ATA2 15

void pic_init(void);

void pic_send_eoi(uint8_t irq);

void pic_set_mask(uint8_t irq);

void pic_clear_mask(uint8_t irq);

uint16_t pic_get_mask(void);

void pic_set_full_mask(uint16_t mask);

uint16_t pic_get_irr(void);

uint16_t pic_get_isr(void);

void pic_disable(void);

void pic_remap(uint8_t offset1, uint8_t offset2);

#endif