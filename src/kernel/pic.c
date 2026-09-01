#include "../include/pic.h"
#include "../include/io.h"
#include "../include/serial.h"

#define ICW1_INIT    0x11
#define ICW4_8086    0x01

void pic_init(void) {
    // Save interrupt masks
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    // ICW1: Start initialization sequence in cascade mode
    outb(PIC1_COMMAND, ICW1_INIT);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT);
    io_wait();

    // ICW2: Vector offsets (Master = 32 [0x20], Slave = 40 [0x28])
    outb(PIC1_DATA, 0x20);
    io_wait();
    outb(PIC2_DATA, 0x28);
    io_wait();

    // ICW3: Tell Master PIC there is a slave at IRQ2 (0000 0100b = 0x04)
    outb(PIC1_DATA, 0x04);
    io_wait();
    // Tell Slave PIC its cascade identity (0000 0010b = 0x02)
    outb(PIC2_DATA, 0x02);
    io_wait();

    // ICW4: Set 8086/88 microprocessor mode
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    // Restore saved masks (or mask all except IRQ0 timer & IRQ1 keyboard)
    (void)mask1;
    (void)mask2;
    // Enable IRQ0 (Timer), IRQ1 (Keyboard), and IRQ2 (Cascade)
    outb(PIC1_DATA, 0xF8); // 1111 1000 -> unmask IRQ0, IRQ1, IRQ2
    outb(PIC2_DATA, 0xFF); // mask all slave IRQs for now

    serial_puts("[PIC] 8259 PIC remapped to vectors 0x20-0x2F (32-47).\n");
}

void pic_send_eoi(uint8_t irq) {
    // If the interrupt came from the Slave PIC (IRQ 8-15), send EOI to Slave
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    // Always send EOI to Master PIC
    outb(PIC1_COMMAND, PIC_EOI);
}

void pic_mask_irq(uint8_t irq) {
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

void pic_unmask_irq(uint8_t irq) {
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

void pic_disable(void) {
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}
