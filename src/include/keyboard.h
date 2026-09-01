#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "types.h"

#define KEYBOARD_DATA_PORT   0x60
#define KEYBOARD_STATUS_PORT 0x64

/**
 * Initialize PS/2 Keyboard driver and register interrupt handler on IRQ1 (Vector 33).
 */
void keyboard_init(void);

/**
 * Handle incoming scancode from PS/2 data stream.
 */
void keyboard_handle_scancode(uint8_t scancode);

#endif // KEYBOARD_H
