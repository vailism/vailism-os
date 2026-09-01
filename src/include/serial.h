#ifndef SERIAL_H
#define SERIAL_H

#include "types.h"

#define COM1_PORT 0x3F8

/**
 * serial_init: Configure the 16550 UART on COM1 (0x3F8).
 * Sets baud rate to 38400 baud, 8 bits, no parity, 1 stop bit.
 * Returns 0 on success, non-zero on failure.
 */
int serial_init(void);

/**
 * serial_putchar: Transmit a single character over COM1.
 */
void serial_putchar(char c);

/**
 * serial_puts: Transmit a null-terminated string over COM1.
 */
void serial_puts(const char *str);

#endif // SERIAL_H
