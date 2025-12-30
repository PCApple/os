#include "include/pic.h"

// Sends the end-of-interrupt signal to the PIC
// @param irq: the IRQ line to send the EOI for
void pic_send_eoi(uint8_t irq)
{
    if (irq > 7) {
        outb(PIC_EOI, PIC2_COMMAND);
    }
    outb(PIC_EOI, PIC1_COMMAND);
}
// Remaps the PICs to the specified offsets
// @param offset1: vector offset for master PIC
// @param offset2: vector offset for slave PIC
void pic_remap(int offset1, int offset2)
{
	outb(ICW1_INIT | ICW1_ICW4,PIC1_COMMAND);  // starts the initialization sequence (in cascade mode)
    io_wait();
	outb(ICW1_INIT | ICW1_ICW4,PIC2_COMMAND);
    io_wait();
	outb(offset1, PIC1_DATA);                 // ICW2: Master PIC vector offset
    io_wait();
	outb(offset2, PIC2_DATA);                 // ICW2: Slave PIC vector offset
    io_wait();
	outb(1 << CASCADE_IRQ, PIC1_DATA);        // ICW3: tell Master PIC that there is a slave PIC at IRQ2
    io_wait();
	outb(2, PIC2_DATA);                       // ICW3: tell Slave PIC its cascade identity (0000 0010)
    io_wait();

	outb(ICW4_8086, PIC1_DATA);               // ICW4: have the PICs use 8086 mode (and not 8080 mode)
    io_wait();
	outb(ICW4_8086, PIC2_DATA);
    io_wait();

	// Unmask both PICs.
	outb(0, PIC1_DATA);
	outb(0, PIC2_DATA);
}

// restores the PICs to the default state
void PIC_disable(void)
{
    outb(0xff, PIC1_DATA);
    outb(0xff, PIC2_DATA);
}

// Masks the specified IRQ line so it can be ignored
// @param IRQline: the IRQ line to mask
void IRQ_set_mask(uint8_t IRQline)
{
    uint16_t port;
    uint8_t value;

    if(IRQline < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        IRQline -= 8;
    }
    value = inb(port) | (1 << IRQline);
    outb(value, port);
}
// Clears the mask on the specified IRQ line
// @param IRQline: the IRQ line to clear
void IRQ_clear_mask(uint8_t IRQline)
{
    uint16_t port;
    uint8_t value;

    if(IRQline < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        IRQline -= 8;
    }
    value = inb(port) & ~(1 << IRQline);
    outb(value, port);
}

/* Helper func */
static uint16_t __pic_get_irq_reg(int ocw3)
{
    /* OCW3 to PIC CMD to get the register values.  PIC2 is chained, and
     * represents IRQs 8-15.  PIC1 is IRQs 0-7, with 2 being the chain */
    outb(ocw3, PIC1_COMMAND);
    outb(ocw3, PIC2_COMMAND);
    return (inb(PIC2_COMMAND) << 8) | inb(PIC1_COMMAND);
}

/* Returns the combined value of the cascaded PICs irq request register */
uint16_t pic_get_irr(void)
{
    return __pic_get_irq_reg(PIC_READ_IRR);
}

/* Returns the combined value of the cascaded PICs in-service register */
uint16_t pic_get_isr(void)
{
    return __pic_get_irq_reg(PIC_READ_ISR);
}

// Initializes the PIC with the offset of Master: 0x20, Slave: 0x28
void PIC_init()
{
    //pushf();
    //cli();
    pic_remap(0x20, 0x28);
    //popf();
}