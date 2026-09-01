#include "../include/types.h"
#include "../include/limine.h"
#include "../include/serial.h"
#include "../include/framebuffer.h"

// 1. Tell Limine we support Base Revision 3
__attribute__((used, section(".requests")))
static volatile LIMINE_BASE_REVISION(3);

// 2. Request start marker (Limine v8 protocol requirement)
__attribute__((used, section(".requests_start_marker")))
static volatile LIMINE_REQUESTS_START_MARKER;

// 3. Framebuffer request: ask bootloader to initialize linear RGB graphics
__attribute__((used, section(".requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

// 4. Request end marker
__attribute__((used, section(".requests_end_marker")))
static volatile LIMINE_REQUESTS_END_MARKER;

/**
 * Kernel Entry Point: kmain
 * 
 * Called directly by Limine in 64-bit Long Mode (Ring 0).
 */
void kmain(void) {
    // Check if the bootloader understood our base revision
    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        for (;;) {
            __asm__ volatile ("hlt");
        }
    }

    // Initialize serial port for debug logging to host terminal
    serial_init();
    serial_puts("\n========================================\n");
    serial_puts("       Welcome to Vailism OS\n");
    serial_puts("========================================\n");
    serial_puts("[KERNEL] Booted successfully in 64-bit Long Mode (Ring 0).\n");

    // Initialize graphical framebuffer
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        serial_puts("[ERROR] No framebuffer provided by bootloader!\n");
        for (;;) {
            __asm__ volatile ("hlt");
        }
    }

    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    fb_init(fb);

    // Render modern welcome banner on screen
    fb_set_color(FB_COLOR_CYAN, FB_COLOR_BG);
    fb_puts("========================================================\n");
    fb_puts("                 Welcome to Vailism OS                  \n");
    fb_puts("========================================================\n\n");

    fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
    fb_puts("[ OK ] ");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    fb_puts("CPU running in 64-bit Long Mode (Supervisor / Ring 0)\n");

    fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
    fb_puts("[ OK ] ");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    fb_puts("Serial Port (COM1 0x3F8) initialized at 38400 baud\n");

    fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
    fb_puts("[ OK ] ");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    fb_puts("Linear graphical framebuffer initialized (32 bpp)\n\n");

    fb_set_color(FB_COLOR_YELLOW, FB_COLOR_BG);
    fb_puts("Phase 1 Milestone Complete!\n");
    fb_set_color(FB_COLOR_MUTED, FB_COLOR_BG);
    fb_puts("Ready for Phase 2: GDT, IDT, Exceptions & Interrupts.\n\n");

    serial_puts("[KERNEL] Framebuffer terminal rendered successfully.\n");
    serial_puts("[KERNEL] Phase 1 Milestone Complete. Entering halt loop.\n");

    // Halt CPU loop (low power state waiting for interrupts)
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
