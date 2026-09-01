#include "../include/mouse.h"
#include "../include/idt.h"
#include "../include/pic.h"
#include "../include/io.h"
#include "../include/serial.h"

static mouse_state_t g_mouse_state = {0, 0, false, false, false};
static uint32_t g_screen_w = 1280;
static uint32_t g_screen_h = 720;

static uint8_t g_mouse_cycle = 0;
static uint8_t g_mouse_packet[3];

static void mouse_wait_write(void) {
    uint32_t timeout = 100000;
    while (timeout--) {
        if ((inb(0x64) & 0x02) == 0) return;
    }
}

static void mouse_wait_read(void) {
    uint32_t timeout = 100000;
    while (timeout--) {
        if ((inb(0x64) & 0x01) == 1) return;
    }
}

static void mouse_write(uint8_t val) {
    mouse_wait_write();
    outb(0x64, 0xD4); // Tell 8042 controller next byte goes to mouse device
    mouse_wait_write();
    outb(0x60, val);
}

static uint8_t mouse_read(void) {
    mouse_wait_read();
    return inb(0x60);
}

static void mouse_callback(struct registers *regs) {
    (void)regs;
    uint8_t status = inb(0x64);
    if ((status & 0x01) == 0) return;
    if ((status & 0x20) == 0) return; // Discard if byte is not from mouse (aux device)

    uint8_t mouse_in = inb(0x60);

    switch (g_mouse_cycle) {
        case 0:
            // First byte: Bit 3 is always 1 in valid PS/2 packets
            if ((mouse_in & 0x08) == 0x08) {
                g_mouse_packet[0] = mouse_in;
                g_mouse_cycle++;
            }
            break;

        case 1:
            g_mouse_packet[1] = mouse_in;
            g_mouse_cycle++;
            break;

        case 2:
            g_mouse_packet[2] = mouse_in;
            g_mouse_cycle = 0;

            // Decode 3-byte packet
            uint8_t flags = g_mouse_packet[0];
            int32_t dx = (int32_t)g_mouse_packet[1];
            int32_t dy = (int32_t)g_mouse_packet[2];

            // Sign extension
            if (flags & 0x10) dx |= 0xFFFFFF00;
            if (flags & 0x20) dy |= 0xFFFFFF00;

            // Discard overflow packets
            if (flags & 0xC0) return;

            g_mouse_state.left_button   = (flags & 0x01) != 0;
            g_mouse_state.right_button  = (flags & 0x02) != 0;
            g_mouse_state.middle_button = (flags & 0x04) != 0;

            g_mouse_state.x += dx;
            g_mouse_state.y -= dy; // Inverted Y-axis on standard PS/2

            // Clamp coordinates to screen boundaries
            if (g_mouse_state.x < 0) g_mouse_state.x = 0;
            if (g_mouse_state.x >= (int32_t)g_screen_w) g_mouse_state.x = g_screen_w - 1;

            if (g_mouse_state.y < 0) g_mouse_state.y = 0;
            if (g_mouse_state.y >= (int32_t)g_screen_h) g_mouse_state.y = g_screen_h - 1;
            break;
    }
}

void mouse_init(uint32_t screen_width, uint32_t screen_height) {
    g_screen_w = screen_width;
    g_screen_h = screen_height;
    g_mouse_state.x = screen_width / 2;
    g_mouse_state.y = screen_height / 2;

    // 0. Drain any stale bytes from port 0x60 to prevent packet desync
    while (inb(0x64) & 0x01) {
        inb(0x60);
    }

    // 1. Enable Auxiliary Mouse Device on 8042 controller
    mouse_wait_write();
    outb(0x64, 0xA8);

    // 2. Enable Interrupts for mouse in Compaq Status Byte
    mouse_wait_write();
    outb(0x64, 0x20); // Read Command Byte
    uint8_t status = mouse_read();
    status |= 0x02;   // Enable IRQ12
    status &= ~0x20;  // Enable mouse clock line

    mouse_wait_write();
    outb(0x64, 0x60); // Write Command Byte
    mouse_wait_write();
    outb(0x60, status);

    // 3. Set Mouse Defaults (Command 0xF6)
    mouse_write(0xF6);
    mouse_read(); // Read ACK (0xFA)

    // 4. Enable Packet Streaming (Command 0xF4)
    mouse_write(0xF4);
    mouse_read(); // Read ACK (0xFA)

    // 5. Register IRQ12 Handler (Vector 44)
    register_interrupt_handler(44, mouse_callback);

    // 6. Unmask IRQ12 on Slave PIC and IRQ2 on Master PIC
    pic_unmask_irq(2);
    pic_unmask_irq(12);

    serial_puts("[MOUSE] PS/2 Mouse driver initialized on IRQ12 (Vector 44).\n");
}

mouse_state_t mouse_get_state(void) {
    return g_mouse_state;
}
