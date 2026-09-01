#include "../include/keyboard.h"
#include "../include/mouse.h"
#include "../include/ps2.h"
#include "../include/idt.h"
#include "../include/pic.h"
#include "../include/io.h"
#include "../include/serial.h"
#include "../include/shell.h"
#include "../include/string.h"

static bool g_shift_pressed = false;
static bool g_ctrl_pressed __attribute__((unused)) = false;
static bool g_alt_pressed __attribute__((unused)) = false;
static bool g_caps_lock = false;

// Diagnostics
static uint32_t g_kbd_diag_count = 0;
#define KBD_DIAG_LIMIT 10

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

void keyboard_handle_scancode(uint8_t scancode) {
    if (g_kbd_diag_count < KBD_DIAG_LIMIT) {
        g_kbd_diag_count++;
        char kdiag[64];
        ksnprintf(kdiag, sizeof(kdiag), "[KBD EVENT #%u] scancode=0x%x\n", (uint64_t)g_kbd_diag_count, (uint64_t)scancode);
        serial_puts(kdiag);
    }

    // Key Release (Break Code: bit 7 set)
    if (scancode & 0x80) {
        uint8_t released_code = scancode & 0x7F;
        if (released_code == 0x2A || released_code == 0x36) { // Left/Right Shift released
            g_shift_pressed = false;
        } else if (released_code == 0x1D) { // Ctrl released
            g_ctrl_pressed = false;
        } else if (released_code == 0x38) { // Alt released
            g_alt_pressed = false;
        }
        return;
    }

    // Key Press (Make Code: bit 7 clear)
    switch (scancode) {
        case 0x2A: // Left Shift pressed
        case 0x36: // Right Shift pressed
            g_shift_pressed = true;
            return;
        case 0x1D: // Left Ctrl pressed
            g_ctrl_pressed = true;
            return;
        case 0x38: // Left Alt pressed
            g_alt_pressed = true;
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
        shell_handle_char(c);
    }
}

static void keyboard_callback(struct registers *regs) {
    (void)regs;
    uint8_t status = inb(PS2_STATUS_PORT);
    if ((status & PS2_STATUS_OUTPUT_BUFFER_FULL) == 0) return;

    uint8_t data = inb(PS2_DATA_PORT);

    // If status bit 5 is set, data belongs to the Mouse auxiliary device
    if (status & PS2_STATUS_AUX_DATA) {
        mouse_handle_byte(data);
        return;
    }

    keyboard_handle_scancode(data);
}

void keyboard_init(void) {
    g_shift_pressed = false;
    g_ctrl_pressed = false;
    g_alt_pressed = false;
    g_caps_lock = false;
    g_kbd_diag_count = 0;

    // Register interrupt handler for IRQ1 (Interrupt Vector 33)
    register_interrupt_handler(33, keyboard_callback);

    // Unmask IRQ1 on Master PIC
    pic_unmask_irq(1);

    serial_puts("[KEYBOARD] PS/2 Keyboard driver registered on IRQ1 (Vector 33).\n");
}
