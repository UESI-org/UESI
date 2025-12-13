#include <pic.h>
#include <io.h>

/**
 * Initialize the 8259 PIC chips in cascade mode
 *
 * This configures the master and slave PICs with proper interrupt remapping
 * to avoid conflicts with CPU exceptions (0-31). The PICs are chained together
 * via IRQ2 on the master, allowing handling of 15 hardware interrupts total.
 */
void
pic_init(void)
{
	pic_remap(PIC1_OFFSET, PIC2_OFFSET);
}

void
pic_remap(uint8_t offset1, uint8_t offset2)
{
	uint8_t mask1, mask2;

	mask1 = inb(PIC1_DATA);
	mask2 = inb(PIC2_DATA);

	outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
	io_wait();
	outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
	io_wait();

	outb(PIC1_DATA, offset1);
	io_wait();
	outb(PIC2_DATA, offset2);
	io_wait();

	outb(PIC1_DATA, 0x04); // 0000 0100 - IRQ2 has slave
	io_wait();
	outb(PIC2_DATA, 0x02); // 0000 0010 - slave identity
	io_wait();

	outb(PIC1_DATA, ICW4_8086);
	io_wait();
	outb(PIC2_DATA, ICW4_8086);
	io_wait();

	// Restore saved masks
	outb(PIC1_DATA, mask1);
	outb(PIC2_DATA, mask2);
}

void
pic_send_eoi(uint8_t irq)
{
	// If IRQ came from slave PIC, send EOI to both
	if (irq >= 8) {
		outb(PIC2_COMMAND, PIC_EOI);
	}
	outb(PIC1_COMMAND, PIC_EOI);
}

void
pic_set_mask(uint8_t irq)
{
	uint16_t port;
	uint8_t value;

	if (irq < 8) {
		port = PIC1_DATA;
	} else {
		port = PIC2_DATA;
		irq -= 8;
	}

	value = inb(port) | (1 << irq);
	outb(port, value);
}

void
pic_clear_mask(uint8_t irq)
{
	uint16_t port;
	uint8_t value;

	if (irq < 8) {
		port = PIC1_DATA;
	} else {
		port = PIC2_DATA;
		irq -= 8;
	}

	value = inb(port) & ~(1 << irq);
	outb(port, value);
}

uint16_t
pic_get_mask(void)
{
	return (inb(PIC2_DATA) << 8) | inb(PIC1_DATA);
}

void
pic_set_full_mask(uint16_t mask)
{
	outb(PIC1_DATA, mask & 0xFF);
	outb(PIC2_DATA, (mask >> 8) & 0xFF);
}

uint16_t
pic_get_irr(void)
{
	outb(PIC1_COMMAND, PIC_READ_IRR);
	outb(PIC2_COMMAND, PIC_READ_IRR);
	return (inb(PIC2_COMMAND) << 8) | inb(PIC1_COMMAND);
}

uint16_t
pic_get_isr(void)
{
	outb(PIC1_COMMAND, PIC_READ_ISR);
	outb(PIC2_COMMAND, PIC_READ_ISR);
	return (inb(PIC2_COMMAND) << 8) | inb(PIC1_COMMAND);
}

void
pic_disable(void)
{
	// Mask all interrupts on both PICs
	outb(PIC1_DATA, 0xFF);
	outb(PIC2_DATA, 0xFF);
}