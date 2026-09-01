#include "../include/serial.h"
#include "../include/io.h"

/**
 * 16550 UART Port Registers (relative to COM1 base 0x3F8):
 *  +0: Data register (R/W) or Divisor Latch Low (if DLAB set)
 *  +1: Interrupt Enable Register (IER) or Divisor Latch High (if DLAB set)
 *  +2: FIFO Control Register (FCR) / Interrupt Identification Register (IIR)
 *  +3: Line Control Register (LCR) -> bit 7 is DLAB (Divisor Latch Access Bit)
 *  +4: Modem Control Register (MCR)
 *  +5: Line Status Register (LSR) -> bit 5 is Transmitter Holding Register Empty
 */

int serial_init(void) {
    outb(COM1_PORT + 1, 0x00);    // Disable all interrupts
    outb(COM1_PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(COM1_PORT + 0, 0x03);    // Set divisor to 3 (lo byte) -> 38400 baud (115200 / 3)
    outb(COM1_PORT + 1, 0x00);    //                  (hi byte)
    outb(COM1_PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(COM1_PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(COM1_PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
    outb(COM1_PORT + 4, 0x1E);    // Set in loopback mode, test the serial chip
    outb(COM1_PORT + 0, 0xAE);    // Test send byte 0xAE

    // Check if serial is faulty (i.e. byte read back does not match what was sent)
    if (inb(COM1_PORT + 0) != 0xAE) {
        return 1;
    }

    // If serial is not faulty, set it to normal operation mode
    // (not-loopback with IRQs enabled and OUT#1 and OUT#2 bits enabled)
    outb(COM1_PORT + 4, 0x0F);
    return 0;
}

static int is_transmit_empty(void) {
    return inb(COM1_PORT + 5) & 0x20;
}

void serial_putchar(char c) {
    // Wait until transmitter is empty
    while (is_transmit_empty() == 0);
    outb(COM1_PORT, (uint8_t)c);
}

void serial_puts(const char *str) {
    if (!str) return;
    for (size_t i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') {
            serial_putchar('\r');
        }
        serial_putchar(str[i]);
    }
}
