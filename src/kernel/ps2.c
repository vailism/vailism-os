#include "../include/ps2.h"
#include "../include/keyboard.h"
#include "../include/mouse.h"
#include "../include/idt.h"
#include "../include/pic.h"
#include "../include/io.h"
#include "../include/serial.h"
#include "../include/string.h"

void ps2_wait_write(void) {
    uint32_t timeout = 100000;
    while (timeout--) {
        if ((inb(PS2_STATUS_PORT) & PS2_STATUS_INPUT_BUFFER_FULL) == 0) return;
        io_wait();
    }
}

static void ps2_wait_read(void) {
    uint32_t timeout = 100000;
    while (timeout--) {
        if ((inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_BUFFER_FULL) != 0) return;
        io_wait();
    }
}

uint8_t ps2_read_data(void) {
    ps2_wait_read();
    return inb(PS2_DATA_PORT);
}

void ps2_mouse_write(uint8_t val) {
    ps2_wait_write();
    outb(PS2_COMMAND_PORT, 0xD4); // Route next byte to second PS/2 port (auxiliary mouse)
    ps2_wait_write();
    outb(PS2_DATA_PORT, val);
}

void ps2_init(void) {
    serial_puts("[PS2] Initializing 8042 PS/2 Controller...\n");

    // 1. Disable both PS/2 ports during configuration
    ps2_wait_write();
    outb(PS2_COMMAND_PORT, 0xAD); // Disable Keyboard
    ps2_wait_write();
    outb(PS2_COMMAND_PORT, 0xA7); // Disable Mouse

    // 2. Flush output buffer
    for (int i = 0; i < 64 && (inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_BUFFER_FULL); i++) {
        inb(PS2_DATA_PORT);
        io_wait();
    }

    // 3. Read Controller Configuration Byte (Command 0x20)
    ps2_wait_write();
    outb(PS2_COMMAND_PORT, 0x20);
    uint8_t config = ps2_read_data();

    char diag[64];
    ksnprintf(diag, sizeof(diag), "[PS2] Original controller config: 0x%x\n", (uint64_t)config);
    serial_puts(diag);

    // 4. Configure: Enable IRQ1 (bit 0), IRQ12 (bit 1), Translation (bit 6), Enable clocks (clear bits 4 & 5)
    config |= 0x03;   // Enable IRQ1 and IRQ12 interrupts
    config |= 0x40;   // Enable port 1 translation (Scan Code Set 1 translation)
    config &= ~0x30;  // Clear bit 4 (enable keyboard clock) and bit 5 (enable mouse clock)

    // 5. Write back Controller Configuration Byte (Command 0x60)
    ps2_wait_write();
    outb(PS2_COMMAND_PORT, 0x60);
    ps2_wait_write();
    outb(PS2_DATA_PORT, config);

    ksnprintf(diag, sizeof(diag), "[PS2] Configured controller config: 0x%x\n", (uint64_t)config);
    serial_puts(diag);

    // 6. Enable both PS/2 ports
    ps2_wait_write();
    outb(PS2_COMMAND_PORT, 0xAE); // Enable Keyboard port
    ps2_wait_write();
    outb(PS2_COMMAND_PORT, 0xA8); // Enable Mouse port

    // 7. Initialize Keyboard and Mouse sub-drivers
    keyboard_init();
    mouse_init(1280, 800);

    serial_puts("[PS2] 8042 Controller initialized with Keyboard (IRQ1) and Mouse (IRQ12) active.\n");
}
