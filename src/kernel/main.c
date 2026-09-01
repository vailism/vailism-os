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
#include "../include/scheduler.h"
#include "../include/ata.h"
#include "../include/vfs.h"
#include "../include/syscall.h"
#include "../include/shell.h"
#include "../include/gui.h"

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

// 4. HHDM request
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

// -----------------------------------------------------------------------------
// Multitasking Demonstration Worker Threads
// -----------------------------------------------------------------------------

static void worker_task_alpha(void *arg) {
    (void)arg;
    for (int i = 1; i <= 3; i++) {
        serial_puts("[THREAD: Alpha] Periodic background cycle ");
        print_dec_serial(i);
        serial_puts("/3\n");
        thread_sleep(300);
    }
    serial_puts("[THREAD: Alpha] Task finished.\n");
}

static void worker_task_beta(void *arg) {
    (void)arg;
    for (int i = 1; i <= 3; i++) {
        serial_puts("[THREAD: Beta ] Periodic background cycle ");
        print_dec_serial(i);
        serial_puts("/3\n");
        thread_sleep(500);
    }
    serial_puts("[THREAD: Beta ] Task finished.\n");
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
    idt_init();
    pic_init();
    timer_init(100);
    keyboard_init();

    fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
    fb_puts("[ OK ] ");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    fb_puts("GDT, IDT (256 gates), PIC, 100Hz PIT Timer, and PS/2 Keyboard active\n");

    // 6. Phase 3: Memory Management Initializations
    if (memmap_request.response == NULL || hhdm_request.response == NULL) {
        serial_puts("[ERROR] Memmap or HHDM request failed!\n");
        for (;;) {
            __asm__ volatile ("hlt");
        }
    }

    pmm_init(memmap_request.response, hhdm_request.response->offset);
    vmm_init();
    heap_init();

    fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
    fb_puts("[ OK ] ");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    fb_puts("PMM bitmap allocator, 4-level VMM paging, and Dynamic Kernel Heap active\n");

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
    fb_puts("Memory: ");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    fb_puts("Total: "); print_dec_fb(total_mb); fb_puts(" MB | Free: "); print_dec_fb(free_mb); fb_puts(" MB\n");

    // 7. Phase 4: Multitasking & Scheduler
    scheduler_init();
    thread_create("worker_alpha", worker_task_alpha, NULL);
    thread_create("worker_beta",  worker_task_beta,  NULL);

    fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
    fb_puts("[ OK ] ");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    fb_puts("Preemptive Round-Robin Multitasking active (Threads Alpha & Beta spawned)\n");

    // 8. Phase 5: Storage & Virtual Filesystem (VFS)
    ata_init();
    vfs_init();

    fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
    fb_puts("[ OK ] ");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    fb_puts("ATA/IDE storage driver & Virtual Filesystem (VFS) mounted at /\n\n");

    // Test Reading /etc/os-release through standard VFS open/read
    int fd = vfs_open("/etc/os-release", O_RDONLY);
    if (fd >= 0) {
        char buffer[256];
        int64_t bytes = vfs_read(fd, buffer, sizeof(buffer) - 1);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            serial_puts("\n[VFS TEST] Read /etc/os-release:\n");
            serial_puts(buffer);
            serial_puts("\n");

            fb_set_color(FB_COLOR_PURPLE, FB_COLOR_BG);
            fb_puts("VFS File Test: /etc/os-release Content:\n");
            fb_set_color(FB_COLOR_MUTED, FB_COLOR_BG);
            fb_puts(buffer);
            fb_puts("\n");
        }
        vfs_close(fd);
    }

    // Test Creating & Writing /home/demo.txt
    int write_fd = vfs_open("/home/demo.txt", O_CREAT | O_WRONLY);
    if (write_fd >= 0) {
        const char *msg = "Vailism OS VFS Read/Write verification PASSED!\n";
        vfs_write(write_fd, msg, strlen(msg));
        vfs_close(write_fd);

        // Read it back
        int read_fd = vfs_open("/home/demo.txt", O_RDONLY);
        if (read_fd >= 0) {
            char check_buf[128];
            int64_t b = vfs_read(read_fd, check_buf, sizeof(check_buf) - 1);
            if (b > 0) {
                check_buf[b] = '\0';
                serial_puts("[VFS TEST] Created & Verified /home/demo.txt: ");
                serial_puts(check_buf);

                fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
                fb_puts("[ TEST ] ");
                fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
                fb_puts(check_buf);
                fb_puts("\n");
            }
            vfs_close(read_fd);
        }
    }

    // 9. Phase 6: System Call Interface & Interactive Shell
    syscall_init();

    // 10. Phase 7: Desktop GUI, Window Manager & PS/2 Mouse
    gui_init(fb);

    fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
    fb_puts("[ OK ] ");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    fb_puts("Desktop GUI Compositor, Window Manager, and PS/2 Mouse active\n\n");

    // 11. Enable CPU Interrupts (STI)
    __asm__ volatile ("sti");
    serial_puts("[CPU] Interrupts enabled globally (STI executed).\n");

    fb_set_color(FB_COLOR_YELLOW, FB_COLOR_BG);
    fb_puts("Phase 7 Complete: Full x86_64 Operating System Stack Operational!\n");
    fb_set_color(FB_COLOR_MUTED, FB_COLOR_BG);
    fb_puts("Type 'help' for commands, or type 'gui' to launch the Desktop GUI:\n\n");

    // Initialize Interactive Shell
    shell_init();

    serial_puts("\n[KERNEL] System fully initialized. Interactive shell active.\n");

    // Main idle loop (Thread 0)
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
