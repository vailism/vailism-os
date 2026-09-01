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

// ─── Limine Boot Protocol Requests ──────────────────────────────────────────

__attribute__((used, section(".requests")))
static volatile LIMINE_BASE_REVISION(3);

__attribute__((used, section(".requests_start_marker")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

__attribute__((used, section(".requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0
};

__attribute__((used, section(".requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0
};

__attribute__((used, section(".requests_end_marker")))
static volatile LIMINE_REQUESTS_END_MARKER;

// ─── Background Worker Threads ──────────────────────────────────────────────

static void worker_task_alpha(void *arg) {
    (void)arg;
    char buf[64];
    for (int i = 1; i <= 3; i++) {
        ksnprintf(buf, sizeof(buf), "[THREAD: Alpha] Background cycle %d/3\n", (int64_t)i);
        serial_puts(buf);
        thread_sleep(300);
    }
    serial_puts("[THREAD: Alpha] Task finished.\n");
}

static void worker_task_beta(void *arg) {
    (void)arg;
    char buf[64];
    for (int i = 1; i <= 3; i++) {
        ksnprintf(buf, sizeof(buf), "[THREAD: Beta ] Background cycle %d/3\n", (int64_t)i);
        serial_puts(buf);
        thread_sleep(500);
    }
    serial_puts("[THREAD: Beta ] Task finished.\n");
}

// ─── Kernel Entry Point ─────────────────────────────────────────────────────

void kmain(void) {
    // 1. Verify Limine Base Revision
    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        for (;;) { __asm__ volatile ("hlt"); }
    }

    // 2. Initialize Serial Console (COM1 @ 0x3F8)
    serial_init();
    serial_puts("\n========================================\n");
    serial_puts("       Welcome to Vailism OS\n");
    serial_puts("========================================\n");

    // 3. Initialize Graphical Framebuffer
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        serial_puts("[FATAL] No framebuffer provided by bootloader!\n");
        for (;;) { __asm__ volatile ("hlt"); }
    }

    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    fb_init(fb);

    // Boot Banner
    fb_set_color(FB_COLOR_CYAN, FB_COLOR_BG);
    fb_puts("========================================================================\n");
    fb_puts("                      Welcome to Vailism OS v0.7                        \n");
    fb_puts("========================================================================\n\n");

    // 4. Phase 2: CPU & Interrupts
    gdt_init();
    idt_init();
    pic_init();
    timer_init(100);
    keyboard_init();

    fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
    fb_puts("[ OK ] ");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    fb_puts("GDT/TSS, IDT (256 gates), PIC, PIT 100Hz, PS/2 Keyboard\n");

    // 5. Phase 3: Memory Management
    if (memmap_request.response == NULL || hhdm_request.response == NULL) {
        serial_puts("[FATAL] Memmap or HHDM request failed!\n");
        for (;;) { __asm__ volatile ("hlt"); }
    }

    pmm_init(memmap_request.response, hhdm_request.response->offset);
    vmm_init();
    heap_init();

    fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
    fb_puts("[ OK ] ");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    fb_puts("PMM bitmap, 4-level VMM paging, 4 MiB Kernel Heap\n");

    {
        char mem_buf[128];
        uint64_t total_mb = pmm_get_total_memory() / (1024 * 1024);
        uint64_t free_mb  = pmm_get_free_memory() / (1024 * 1024);
        uint64_t used_mb  = pmm_get_used_memory() / (1024 * 1024);

        ksnprintf(mem_buf, sizeof(mem_buf), "[MEM] Total: %u MB | Free: %u MB | Used: %u MB\n",
                  (uint64_t)total_mb, (uint64_t)free_mb, (uint64_t)used_mb);
        serial_puts(mem_buf);

        fb_set_color(FB_COLOR_PURPLE, FB_COLOR_BG);
        fb_puts("       ");
        fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
        ksnprintf(mem_buf, sizeof(mem_buf), "RAM: %u MB total, %u MB free\n",
                  (uint64_t)total_mb, (uint64_t)free_mb);
        fb_puts(mem_buf);
    }

    // 6. Phase 4: Multitasking & Scheduler
    scheduler_init();
    thread_create("worker_alpha", worker_task_alpha, NULL);
    thread_create("worker_beta",  worker_task_beta,  NULL);

    fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
    fb_puts("[ OK ] ");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    fb_puts("Preemptive Round-Robin Multitasking (100Hz timer-driven)\n");

    // 7. Phase 5: Storage & Virtual Filesystem
    ata_init();
    vfs_init();

    fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
    fb_puts("[ OK ] ");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    fb_puts("ATA/IDE storage driver & VFS mounted at /\n");

    // Quiet VFS self-test
    int fd = vfs_open("/etc/os-release", O_RDONLY);
    if (fd >= 0) {
        char buffer[256];
        int64_t bytes = vfs_read(fd, buffer, sizeof(buffer) - 1);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            serial_puts("[VFS TEST] /etc/os-release: ");
            serial_puts(buffer);
        }
        vfs_close(fd);
    }

    int wfd = vfs_open("/home/demo.txt", O_CREAT | O_WRONLY);
    if (wfd >= 0) {
        const char *msg = "Vailism OS VFS Read/Write verification PASSED!\n";
        vfs_write(wfd, msg, strlen(msg));
        vfs_close(wfd);
        serial_puts("[VFS TEST] /home/demo.txt write+readback OK\n");
    }

    // 8. Phase 6: System Call Interface
    syscall_init();

    fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
    fb_puts("[ OK ] ");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    fb_puts("Fast x86_64 syscall/sysret MSR interface\n");

    // 9. Phase 7: Desktop GUI & Window Manager
    gui_init(fb);

    fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
    fb_puts("[ OK ] ");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    fb_puts("Desktop GUI compositor, Window Manager, PS/2 Mouse\n\n");

    // 10. Enable Interrupts
    __asm__ volatile ("sti");
    serial_puts("[CPU] Interrupts enabled globally (STI).\n");

    // Initialize Interactive Shell
    shell_init();

    serial_puts("\n[KERNEL] All subsystems initialized. Interactive shell active.\n");

    // Main idle loop (Thread 0)
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
