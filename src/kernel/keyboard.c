#include "../include/keyboard.h"
#include "../include/idt.h"
#include "../include/io.h"
#include "../include/serial.h"
#include "../include/framebuffer.h"

static bool g_shift_pressed = false;
static bool g_caps_lock = false;

// Standard US QWERTY Scancode Set 1 (un-shifted)
static const char scancode_ascii_lowercase[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

// Standard US QWERTY Scancode Set 1 (shifted)
static const char scancode_ascii_uppercase[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

static void keyboard_callback(struct registers *regs) {
    (void)regs;
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    // Key Release (Break Code: bit 7 set)
    if (scancode & 0x80) {
        uint8_t released_code = scancode & 0x7F;
        if (released_code == 0x2A || released_code == 0x36) { // Left/Right Shift released
            g_shift_pressed = false;
        }
        return;
    }

    // Key Press (Make Code: bit 7 clear)
    switch (scancode) {
        case 0x2A: // Left Shift pressed
        case 0x36: // Right Shift pressed
            g_shift_pressed = true;
            return;
        case 0x3A: // Caps Lock pressed
            g_caps_lock = !g_caps_lock;
            return;
        default:
            break;
    }

    char c = 0;
    bool uppercase = (g_shift_pressed ^ g_caps_lock);

    if (scancode < 128) {
        if (uppercase) {
            c = scancode_ascii_uppercase[scancode];
        } else {
            c = scancode_ascii_lowercase[scancode];
        }
    }

    if (c != 0) {
        // Echo character to both graphical screen and serial console!
        fb_putchar(c);
        if (c == '\n') {
            serial_putchar('\r');
        }
        serial_putchar(c);
    }
}

void keyboard_init(void) {
    // Register interrupt handler for IRQ1 (Interrupt Vector 33)
    register_interrupt_handler(33, keyboard_callback);
    serial_puts("[KEYBOARD] PS/2 Keyboard driver registered on IRQ1 (Vector 33).\n");
}
