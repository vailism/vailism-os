#include "../include/mouse.h"
#include "../include/keyboard.h"
#include "../include/ps2.h"
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

void mouse_set_screen_dimensions(uint32_t width, uint32_t height) {
    g_screen_w = width;
    g_screen_h = height;
    if (g_mouse_state.x >= (int32_t)g_screen_w) g_mouse_state.x = g_screen_w / 2;
    if (g_mouse_state.y >= (int32_t)g_screen_h) g_mouse_state.y = g_screen_h / 2;
}

void mouse_handle_byte(uint8_t byte) {
    switch (g_mouse_cycle) {
        case 0:
            // First byte: Bit 3 is always 1 in valid standard PS/2 packets
            if ((byte & 0x08) == 0x08) {
                g_mouse_packet[0] = byte;
                g_mouse_cycle = 1;
            }
            break;

        case 1:
            g_mouse_packet[1] = byte;
            g_mouse_cycle = 2;
            break;

        case 2:
            g_mouse_packet[2] = byte;
            g_mouse_cycle = 0;

            // Decode 3-byte packet
            uint8_t flags = g_mouse_packet[0];
            int32_t dx = (int8_t)g_mouse_packet[1];
            int32_t dy = (int8_t)g_mouse_packet[2];

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

static void mouse_callback(struct registers *regs) {
    (void)regs;
    uint8_t status = inb(PS2_STATUS_PORT);
    if ((status & PS2_STATUS_OUTPUT_BUFFER_FULL) == 0) return;

    uint8_t data = inb(PS2_DATA_PORT);

    // If status bit 5 is clear, data belongs to the Keyboard
    if ((status & PS2_STATUS_AUX_DATA) == 0) {
        keyboard_handle_scancode(data);
        return;
    }

    mouse_handle_byte(data);
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

    // Send command 0xF6 (Set Defaults) to mouse device
    ps2_mouse_write(0xF6);
    uint8_t ack1 = ps2_read_data();
    char diag[64];
    ksnprintf(diag, sizeof(diag), "[MOUSE] Set Defaults ACK: 0x%x\n", (uint64_t)ack1);
    serial_puts(diag);

    // Send command 0xF4 (Enable Data Reporting) to mouse device
    ps2_mouse_write(0xF4);
    uint8_t ack2 = ps2_read_data();
    ksnprintf(diag, sizeof(diag), "[MOUSE] Enable Streaming ACK: 0x%x\n", (uint64_t)ack2);
    serial_puts(diag);

    // Register IRQ12 Handler (Vector 44)
    register_interrupt_handler(44, mouse_callback);

    // Unmask Cascade IRQ2 on Master PIC and IRQ12 on Slave PIC
    pic_unmask_irq(2);
    pic_unmask_irq(12);

    serial_puts("[MOUSE] PS/2 Mouse driver registered on IRQ12 (Vector 44).\n");
}

mouse_state_t mouse_get_state(void) {
    return g_mouse_state;
}
