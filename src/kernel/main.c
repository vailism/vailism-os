#include "../include/types.h"
#include "../include/limine.h"
#include "../include/serial.h"
#include "../include/framebuffer.h"
#include "../include/gdt.h"
#include "../include/idt.h"
#include "../include/pic.h"
#include "../include/timer.h"
#include "../include/keyboard.h"
#include "../include/string.h"
#include "../include/pmm.h"
#include "../include/vmm.h"
#include "../include/heap.h"

// 1. Tell Limine we support Base Revision 3
__attribute__((used, section(".requests")))
static volatile LIMINE_BASE_REVISION(3);

// 2. Request start marker
__attribute__((used, section(".requests_start_marker")))
static volatile LIMINE_REQUESTS_START_MARKER;

// 3. Framebuffer request
__attribute__((used, section(".requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

// 4. HHDM (Higher-Half Direct Map) request
__attribute__((used, section(".requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0
};

// 5. Memory map request
__attribute__((used, section(".requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0
};

// 6. Request end marker
__attribute__((used, section(".requests_end_marker")))
static volatile LIMINE_REQUESTS_END_MARKER;

static void print_dec_serial(uint64_t val) {
    char buf[32];
    int i = 0;
    if (val == 0) {
        serial_putchar('0');
        return;
    }
    while (val > 0) {
        buf[i++] = (char)('0' + (val % 10));
        val /= 10;
    }
    for (int j = i - 1; j >= 0; j--) {
        serial_putchar(buf[j]);
    }
}

static void print_dec_fb(uint64_t val) {
    char buf[32];
    int i = 0;
    if (val == 0) {
        fb_putchar('0');
        return;
    }
    while (val > 0) {
        buf[i++] = (char)('0' + (val % 10));
        val /= 10;
    }
    for (int j = i - 1; j >= 0; j--) {
        fb_putchar(buf[j]);
    }
}

/**
 * Kernel Entry Point: kmain
 */
void kmain(void) {
    // 1. Check Limine Base Revision
    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        for (;;) {
            __asm__ volatile ("hlt");
        }
    }

    // 2. Initialize Serial Port COM1 (0x3F8)
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

    // 5. Phase 2: CPU & Interrupts Initializations
    gdt_init();
    fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
    fb_puts("[ OK ] ");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    fb_puts("GDT & 64-bit TSS (Task State Segment) initialized\n");

    idt_init();
    fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
    fb_puts("[ OK ] ");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    fb_puts("IDT initialized with 32 CPU exception vectors (IST1 on Double Fault)\n");

    pic_init();
    fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
    fb_puts("[ OK ] ");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    fb_puts("8259 PIC remapped to vectors 0x20..0x2F (IRQs 0..15)\n");

    timer_init(100);
    fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
    fb_puts("[ OK ] ");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    fb_puts("8254 PIT Timer configured to 100 Hz (IRQ0)\n");

    keyboard_init();
    fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
    fb_puts("[ OK ] ");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    fb_puts("PS/2 Keyboard driver active on IRQ1\n\n");

    // 6. Phase 3: Memory Management Initializations
    if (memmap_request.response == NULL || hhdm_request.response == NULL) {
        serial_puts("[ERROR] Memmap or HHDM request failed!\n");
        for (;;) {
            __asm__ volatile ("hlt");
        }
    }

    // Step A: Physical Memory Manager (PMM)
    pmm_init(memmap_request.response, hhdm_request.response->offset);
    fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
    fb_puts("[ OK ] ");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    fb_puts("PMM (Physical Memory Manager) bitmap allocator initialized\n");

    // Step B: Virtual Memory Manager (VMM)
    vmm_init();
    fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
    fb_puts("[ OK ] ");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    fb_puts("VMM (4-level x86_64 paging) active\n");

    // Step C: Kernel Heap Allocator
    heap_init();
    fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
    fb_puts("[ OK ] ");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    fb_puts("Kernel Heap allocator initialized (kmalloc / kfree available)\n\n");

    // 7. Memory Stats Display
    uint64_t total_mb = pmm_get_total_memory() / (1024 * 1024);
    uint64_t free_mb  = pmm_get_free_memory() / (1024 * 1024);
    uint64_t used_mb  = pmm_get_used_memory() / (1024 * 1024);

    serial_puts("[MEM] Total Physical RAM: ");
    print_dec_serial(total_mb);
    serial_puts(" MB | Free: ");
    print_dec_serial(free_mb);
    serial_puts(" MB | Used: ");
    print_dec_serial(used_mb);
    serial_puts(" MB\n");

    fb_set_color(FB_COLOR_PURPLE, FB_COLOR_BG);
    fb_puts("Memory Statistics:\n");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    fb_puts("  Total Physical RAM: "); print_dec_fb(total_mb); fb_puts(" MB\n");
    fb_puts("  Free Physical RAM:  "); print_dec_fb(free_mb);  fb_puts(" MB\n");
    fb_puts("  Used Physical RAM:  "); print_dec_fb(used_mb);  fb_puts(" MB\n\n");

    // 8. Heap Dynamic Allocation Self-Test
    char *test_str = (char *)kmalloc(64);
    if (test_str) {
        strcpy(test_str, "Dynamic Heap Allocation (kmalloc) verification PASSED!");
        serial_puts("[HEAP TEST] ");
        serial_puts(test_str);
        serial_puts("\n");

        fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
        fb_puts("[ TEST ] ");
        fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
        fb_puts(test_str);
        fb_puts("\n\n");

        kfree(test_str);
    }

    // 9. Enable CPU Interrupts (STI)
    __asm__ volatile ("sti");
    serial_puts("[CPU] Interrupts enabled globally (STI executed).\n");

    fb_set_color(FB_COLOR_YELLOW, FB_COLOR_BG);
    fb_puts("Phase 3 Milestone Complete: Memory Management & Kernel Heap Active!\n");
    fb_set_color(FB_COLOR_MUTED, FB_COLOR_BG);
    fb_puts("Type commands or text below:\n\n");

    // Interactive Prompt
    fb_set_color(FB_COLOR_CYAN, FB_COLOR_BG);
    fb_puts("vailism-os> ");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);

    serial_puts("\n[KERNEL] System ready. Entering main interrupt wait loop.\n");

    // Main low-power interrupt loop
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
