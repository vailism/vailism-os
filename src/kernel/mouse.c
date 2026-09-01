#include "../include/mouse.h"
#include "../include/idt.h"
#include "../include/pic.h"
#include "../include/io.h"
#include "../include/serial.h"
#include "../include/string.h"

static mouse_state_t g_mouse_state = {0, 0, false, false, false};
static uint32_t g_screen_w = 1280;
static uint32_t g_screen_h = 800;

static uint8_t g_mouse_cycle = 0;
static uint8_t g_mouse_packet[3];

static uint32_t g_mouse_diag_count = 0;
#define MOUSE_DIAG_LIMIT 10

static void mouse_wait_write(void) {
    uint32_t timeout = 100000;
    while (timeout--) {
        if ((inb(0x64) & 0x02) == 0) return;
        io_wait();
    }
}

static void mouse_wait_read(void) {
    uint32_t timeout = 100000;
    while (timeout--) {
        if ((inb(0x64) & 0x01) == 1) return;
        io_wait();
    }
}

static void mouse_write(uint8_t val) {
    mouse_wait_write();
    outb(0x64, 0xD4); // Tell 8042 controller next byte goes to mouse auxiliary device
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

    uint8_t mouse_in = inb(0x60);

    switch (g_mouse_cycle) {
        case 0:
            // First byte: Bit 3 is always 1 in valid PS/2 packets
            if ((mouse_in & 0x08) == 0x08) {
                g_mouse_packet[0] = mouse_in;
                g_mouse_cycle = 1;
            }
            break;

        case 1:
            g_mouse_packet[1] = mouse_in;
            g_mouse_cycle = 2;
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

            // Clamp coordinates strictly to screen boundaries
            if (g_mouse_state.x < 0) g_mouse_state.x = 0;
            if (g_mouse_state.x >= (int32_t)g_screen_w) g_mouse_state.x = g_screen_w - 1;

            if (g_mouse_state.y < 0) g_mouse_state.y = 0;
            if (g_mouse_state.y >= (int32_t)g_screen_h) g_mouse_state.y = g_screen_h - 1;

            // Diagnostic logging for first N packets
            if (g_mouse_diag_count < MOUSE_DIAG_LIMIT) {
                g_mouse_diag_count++;
                char mdiag[128];
                ksnprintf(mdiag, sizeof(mdiag), "[MOUSE EVENT #%u] pkt=[0x%x, 0x%x, 0x%x] dx=%d dy=%d pos=(%d, %d) btn=[%d,%d,%d]\n",
                          (uint64_t)g_mouse_diag_count, (uint64_t)flags, (uint64_t)g_mouse_packet[1], (uint64_t)g_mouse_packet[2],
                          (int64_t)dx, (int64_t)dy, (int64_t)g_mouse_state.x, (int64_t)g_mouse_state.y,
                          (int64_t)g_mouse_state.left_button, (int64_t)g_mouse_state.right_button, (int64_t)g_mouse_state.middle_button);
                serial_puts(mdiag);
            }
            break;
    }
}

void mouse_init(uint32_t screen_width, uint32_t screen_height) {
    g_screen_w = screen_width;
    g_screen_h = screen_height;
    g_mouse_state.x = (int32_t)(screen_width / 2);
    g_mouse_state.y = (int32_t)(screen_height / 2);
    g_mouse_state.left_button = false;
    g_mouse_state.right_button = false;
    g_mouse_state.middle_button = false;
    g_mouse_cycle = 0;
    g_mouse_diag_count = 0;

    // 0. Flush any pending data in the 8042 controller buffer
    for (int i = 0; i < 32 && (inb(0x64) & 0x01); i++) {
        inb(0x60);
        io_wait();
    }

    // 1. Enable Auxiliary Mouse Device on 8042 controller (Command 0xA8)
    mouse_wait_write();
    outb(0x64, 0xA8);
    io_wait();

    // 2. Read Controller Command Byte (Command 0x20)
    mouse_wait_write();
    outb(0x64, 0x20);
    uint8_t status = mouse_read();

    // Enable IRQ1 (bit 0) and IRQ12 (bit 1), enable clocks (clear bit 4 & 5)
    status |= 0x03;   // Bit 0 = Keyboard IRQ1, Bit 1 = Mouse IRQ12
    status &= ~0x30;  // Bit 4 = Enable Keyboard Clock, Bit 5 = Enable Mouse Clock

    // Write back Command Byte (Command 0x60)
    mouse_wait_write();
    outb(0x64, 0x60);
    mouse_wait_write();
    outb(0x60, status);
    io_wait();

    // 3. Set Mouse Defaults (Command 0xF6 to auxiliary device)
    mouse_write(0xF6);
    mouse_read(); // Read ACK (0xFA)

    // 4. Enable Packet Streaming (Command 0xF4 to auxiliary device)
    mouse_write(0xF4);
    mouse_read(); // Read ACK (0xFA)

    // 5. Register IRQ12 Handler (Vector 44)
    register_interrupt_handler(44, mouse_callback);

    // 6. Unmask IRQ2 on Master PIC (Cascade) and IRQ12 on Slave PIC
    pic_unmask_irq(2);
    pic_unmask_irq(12);

    serial_puts("[MOUSE] PS/2 Mouse driver initialized on IRQ12 (Vector 44).\n");
}

mouse_state_t mouse_get_state(void) {
    return g_mouse_state;
}
