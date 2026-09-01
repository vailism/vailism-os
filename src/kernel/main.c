#include "../include/types.h"
#include "../include/limine.h"
#include "../include/serial.h"
#include "../include/framebuffer.h"
#include "../include/gdt.h"
#include "../include/idt.h"
#include "../include/pic.h"
#include "../include/timer.h"
#include "../include/keyboard.h"

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
    // 1. Check if the bootloader understood our base revision
    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        for (;;) {
            __asm__ volatile ("hlt");
        }
    }

    // 2. Initialize Serial Port for debug logs (COM1 0x3F8)
    serial_init();
    serial_puts("\n========================================\n");
    serial_puts("       Welcome to Vailism OS\n");
    serial_puts("========================================\n");

    // 3. Initialize Graphical Framebuffer
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        serial_puts("[ERROR] No framebuffer provided by bootloader!\n");
        for (;;) {
            __asm__ volatile ("hlt");
        }
    }

    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    fb_init(fb);

    // 4. Render Banner
    fb_set_color(FB_COLOR_CYAN, FB_COLOR_BG);
    fb_puts("========================================================================\n");
    fb_puts("                          Welcome to Vailism OS                         \n");
    fb_puts("========================================================================\n\n");

    // 5. Phase 2 Hardware & CPU Initializations
    // Step A: Global Descriptor Table (GDT) & Task State Segment (TSS)
    gdt_init();
    fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
    fb_puts("[ OK ] ");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    fb_puts("GDT & 64-bit TSS (Task State Segment) initialized\n");

    // Step B: Interrupt Descriptor Table (IDT) with 32 CPU Exception Handlers
    idt_init();
    fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
    fb_puts("[ OK ] ");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    fb_puts("IDT initialized with 32 CPU exception vectors (IST1 on Double Fault)\n");

    // Step C: 8259 PIC Remapping (Vectors 32-47)
    pic_init();
    fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
    fb_puts("[ OK ] ");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    fb_puts("8259 PIC remapped to vectors 0x20..0x2F (IRQs 0..15)\n");

    // Step D: PIT Timer (100 Hz = 10ms per tick)
    timer_init(100);
    fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
    fb_puts("[ OK ] ");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    fb_puts("8254 PIT Timer configured to 100 Hz (IRQ0)\n");

    // Step E: PS/2 Keyboard Driver (IRQ1)
    keyboard_init();
    fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
    fb_puts("[ OK ] ");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    fb_puts("PS/2 Keyboard driver active on IRQ1\n\n");

    // Step F: Enable CPU Interrupts (STI)
    __asm__ volatile ("sti");
    serial_puts("[CPU] Interrupts enabled globally (STI executed).\n");

    fb_set_color(FB_COLOR_YELLOW, FB_COLOR_BG);
    fb_puts("Phase 2 Milestone Complete: CPU Interrupts & Hardware IRQs Active!\n");
    fb_set_color(FB_COLOR_MUTED, FB_COLOR_BG);
    fb_puts("Try typing on your keyboard below (interactive keyboard echo enabled):\n\n");

    // Interactive prompt
    fb_set_color(FB_COLOR_CYAN, FB_COLOR_BG);
    fb_puts("vailism-os> ");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);

    serial_puts("\n[KERNEL] System ready. Interactive keyboard loop active.\n");

    // Main low-power interrupt wait loop
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
