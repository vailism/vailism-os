#ifndef PS2_H
#define PS2_H

#include "types.h"

#define PS2_DATA_PORT    0x60
#define PS2_STATUS_PORT  0x64
#define PS2_COMMAND_PORT 0x64

// Status register bits
#define PS2_STATUS_OUTPUT_BUFFER_FULL 0x01
#define PS2_STATUS_INPUT_BUFFER_FULL  0x02
#define PS2_STATUS_AUX_DATA           0x20 // 1 = Mouse data, 0 = Keyboard data

/**
 * Centralized initialization of the Intel 8042 PS/2 Controller.
 * Configures both Port 1 (Keyboard on IRQ1) and Port 2 (Mouse on IRQ12).
 */
void ps2_init(void);

void ps2_wait_write(void);
uint8_t ps2_read_data(void);
void ps2_mouse_write(uint8_t val);

#endif // PS2_H
