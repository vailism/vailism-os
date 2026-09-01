#ifndef MOUSE_H
#define MOUSE_H

#include "types.h"

typedef struct {
    int32_t x;
    int32_t y;
    bool    left_button;
    bool    right_button;
    bool    middle_button;
} mouse_state_t;

/**
 * Initialize PS/2 Mouse on IRQ12 (Vector 44).
 */
void mouse_init(uint32_t screen_width, uint32_t screen_height);

/**
 * Handle incoming byte from PS/2 auxiliary stream.
 */
void mouse_handle_byte(uint8_t byte);

/**
 * Update screen dimensions for cursor clamping.
 */
void mouse_set_screen_dimensions(uint32_t width, uint32_t height);

/**
 * Get current mouse state (coordinates and button states).
 */
mouse_state_t mouse_get_state(void);

#endif // MOUSE_H
