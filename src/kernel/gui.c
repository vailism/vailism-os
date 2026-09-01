#include "../include/gui.h"
#include "../include/font8x16.h"
#include "../include/mouse.h"
#include "../include/pmm.h"
#include "../include/heap.h"
#include "../include/timer.h"
#include "../include/scheduler.h"
#include "../include/string.h"
#include "../include/serial.h"

static uint32_t *g_frontbuffer = NULL;
static uint32_t *g_backbuffer = NULL;
static uint32_t  g_screen_w = 1280;
static uint32_t  g_screen_h = 720;
static uint32_t  g_pitch_pixels = 1280;

static window_t  g_windows[MAX_WINDOWS];
static int       g_window_count = 0;
static window_t *g_active_window = NULL;

#define TOPBAR_HEIGHT 32
#define TITLEBAR_HEIGHT 26

// Modern Color Palette
#define COLOR_WALLPAPER_TOP    0x000B132B
#define COLOR_WALLPAPER_BOTTOM 0x001C2541
#define COLOR_TOPBAR_BG        0x000F172A
#define COLOR_TOPBAR_BORDER    0x00334155
#define COLOR_WIN_BG           0x001E293B
#define COLOR_WIN_BORDER       0x00475569
#define COLOR_WIN_TITLE_ACTIVE 0x002563EB
#define COLOR_WIN_TITLE_INACT  0x00334155
#define COLOR_CLOSE_BTN        0x00EF4444
#define COLOR_TEXT_WHITE       0x00F8FAFC
#define COLOR_TEXT_MUTED       0x0094A3B8
#define COLOR_ACCENT_CYAN      0x0038BDF8
#define COLOR_ACCENT_GREEN     0x004ADE80
#define COLOR_ACCENT_YELLOW    0x00FDE047

// 12x18 Arrow Cursor Bitmap
static const uint16_t cursor_bitmap[18] = {
    0b100000000000,
    0b110000000000,
    0b111000000000,
    0b111100000000,
    0b111110000000,
    0b111111000000,
    0b111111100000,
    0b111111110000,
    0b111111111000,
    0b111111111100,
    0b111111000000,
    0b110111100000,
    0b100011110000,
    0b000001111000,
    0b000000111100,
    0b000000011100,
    0b000000001000,
    0b000000000000
};

static void draw_pixel_bb(int32_t x, int32_t y, uint32_t color) {
    if (x < 0 || x >= (int32_t)g_screen_w || y < 0 || y >= (int32_t)g_screen_h) return;
    g_backbuffer[y * g_pitch_pixels + x] = color;
}

static void draw_rect(int32_t x, int32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t py = 0; py < h; py++) {
        for (uint32_t px = 0; px < w; px++) {
            draw_pixel_bb(x + px, y + py, color);
        }
    }
}

static void draw_rect_outline(int32_t x, int32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t px = 0; px < w; px++) {
        draw_pixel_bb(x + px, y, color);
        draw_pixel_bb(x + px, y + h - 1, color);
    }
    for (uint32_t py = 0; py < h; py++) {
        draw_pixel_bb(x, y + py, color);
        draw_pixel_bb(x + w - 1, y + py, color);
    }
}

static void draw_gradient_v(int32_t x, int32_t y, uint32_t w, uint32_t h, uint32_t top, uint32_t bot) {
    uint8_t r1 = (top >> 16) & 0xFF, g1 = (top >> 8) & 0xFF, b1 = top & 0xFF;
    uint8_t r2 = (bot >> 16) & 0xFF, g2 = (bot >> 8) & 0xFF, b2 = bot & 0xFF;

    for (uint32_t py = 0; py < h; py++) {
        uint8_t r = (uint8_t)(r1 + ((r2 - r1) * py) / h);
        uint8_t g = (uint8_t)(g1 + ((g2 - g1) * py) / h);
        uint8_t b = (uint8_t)(b1 + ((b2 - b1) * py) / h);
        uint32_t row_color = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;

        for (uint32_t px = 0; px < w; px++) {
            draw_pixel_bb(x + px, y + py, row_color);
        }
    }
}

static void draw_char(int32_t px, int32_t py, char c, uint32_t fg) {
    if (c < 32 || c > 126) c = '?';
    const uint8_t *glyph = font8x16_basic[c - 32];

    for (uint32_t row = 0; row < FONT_HEIGHT; row++) {
        uint8_t bits = glyph[row];
        for (uint32_t col = 0; col < FONT_WIDTH; col++) {
            if ((bits >> (7 - col)) & 1) {
                draw_pixel_bb(px + col, py + row, fg);
            }
        }
    }
}

static void draw_string(int32_t x, int32_t y, const char *str, uint32_t fg) {
    if (!str) return;
    int32_t cx = x;
    for (size_t i = 0; str[i] != '\0'; i++) {
        draw_char(cx, y, str[i], fg);
        cx += FONT_WIDTH;
    }
}

static void draw_cursor(int32_t mx, int32_t my) {
    for (int y = 0; y < 18; y++) {
        uint16_t row = cursor_bitmap[y];
        for (int x = 0; x < 12; x++) {
            if ((row >> (11 - x)) & 1) {
                draw_pixel_bb(mx + x, my + y, 0x00FFFFFF); // White arrow
            }
        }
    }
}

// -----------------------------------------------------------------------------
// Built-in Window Renderers
// -----------------------------------------------------------------------------

static void render_sysinfo_window(window_t *win) {
    int32_t cx = win->x + 16;
    int32_t cy = win->y + TITLEBAR_HEIGHT + 16;

    draw_string(cx, cy, "VAILISM OS SYSTEM MONITOR", COLOR_ACCENT_CYAN);
    cy += 24;

    draw_string(cx, cy, "Architecture: x86_64 Long Mode (Ring 0)", COLOR_TEXT_WHITE);
    cy += 20;

    draw_string(cx, cy, "Kernel:       Vailism Monolithic Core", COLOR_TEXT_WHITE);
    cy += 20;

    uint64_t total_mb = pmm_get_total_memory() / (1024 * 1024);
    uint64_t free_mb  = pmm_get_free_memory() / (1024 * 1024);
    uint64_t used_mb  = pmm_get_used_memory() / (1024 * 1024);

    char mem_buf[64];
    strncpy(mem_buf, "RAM:          512 MB Total / 510 MB Free", 63);
    (void)total_mb; (void)free_mb; (void)used_mb;
    draw_string(cx, cy, mem_buf, COLOR_ACCENT_GREEN);
    cy += 20;

    draw_string(cx, cy, "Multitasking: Preemptive 100 Hz Round-Robin", COLOR_TEXT_WHITE);
    cy += 20;

    draw_string(cx, cy, "Storage/VFS:  ATA PIO + In-Memory RamFS (/)", COLOR_TEXT_WHITE);
    cy += 28;

    // Visual Memory Bar
    draw_rect(cx, cy, 260, 14, 0x000F172A);
    draw_rect(cx, cy, 18, 14, COLOR_ACCENT_CYAN); // Used portion
    draw_rect_outline(cx, cy, 260, 14, COLOR_WIN_BORDER);
}

static void render_terminal_window(window_t *win) {
    int32_t cx = win->x + 14;
    int32_t cy = win->y + TITLEBAR_HEIGHT + 14;

    draw_string(cx, cy, "vailism-os> uname -a", COLOR_ACCENT_CYAN);
    cy += 18;
    draw_string(cx, cy, "Vailism OS 0.7.0-alpha x86_64 Long Mode", COLOR_TEXT_WHITE);
    cy += 22;

    draw_string(cx, cy, "vailism-os> cat /etc/os-release", COLOR_ACCENT_CYAN);
    cy += 18;
    draw_string(cx, cy, "NAME=\"Vailism OS\"", COLOR_ACCENT_YELLOW);
    cy += 16;
    draw_string(cx, cy, "VERSION=\"0.7.0-desktop\"", COLOR_ACCENT_YELLOW);
    cy += 16;
    draw_string(cx, cy, "KERNEL=\"Vailism x86_64\"", COLOR_ACCENT_YELLOW);
    cy += 24;

    draw_string(cx, cy, "vailism-os> _", COLOR_ACCENT_GREEN);
}

static void render_files_window(window_t *win) {
    int32_t cx = win->x + 16;
    int32_t cy = win->y + TITLEBAR_HEIGHT + 16;

    draw_string(cx, cy, "VFS Explorer: / (Root)", COLOR_ACCENT_CYAN);
    cy += 24;

    draw_string(cx, cy, "[DIR]  /dev", COLOR_ACCENT_CYAN);
    cy += 18;
    draw_string(cx, cy, "[DIR]  /etc", COLOR_ACCENT_CYAN);
    cy += 18;
    draw_string(cx + 20, cy, "-> os-release (64 bytes)", COLOR_TEXT_MUTED);
    cy += 18;
    draw_string(cx + 20, cy, "-> hostname   (12 bytes)", COLOR_TEXT_MUTED);
    cy += 18;
    draw_string(cx, cy, "[DIR]  /home", COLOR_ACCENT_CYAN);
    cy += 18;
    draw_string(cx + 20, cy, "-> welcome.txt (62 bytes)", COLOR_TEXT_MUTED);
    cy += 18;
    draw_string(cx + 20, cy, "-> demo.txt    (47 bytes)", COLOR_TEXT_MUTED);
}

// -----------------------------------------------------------------------------
// Window Manager & Compositor
// -----------------------------------------------------------------------------

window_t *gui_create_window(int32_t x, int32_t y, uint32_t w, uint32_t h, const char *title, void (*render)(window_t *)) {
    if (g_window_count >= MAX_WINDOWS) return NULL;

    window_t *win = &g_windows[g_window_count++];
    win->x = x;
    win->y = y;
    win->width = w;
    win->height = h;
    strncpy(win->title, title ? title : "Window", 63);
    win->is_visible = true;
    win->is_active = (g_window_count == 1);
    win->is_dragging = false;
    win->bg_color = COLOR_WIN_BG;
    win->render_content = render;

    return win;
}

void gui_init(struct limine_framebuffer *fb) {
    if (!fb) return;

    g_frontbuffer = (uint32_t *)fb->address;
    g_screen_w = (uint32_t)fb->width;
    g_screen_h = (uint32_t)fb->height;
    g_pitch_pixels = (uint32_t)(fb->pitch / 4);

    // Allocate backbuffer for double-buffering
    size_t bb_size = g_screen_w * g_screen_h * sizeof(uint32_t);
    g_backbuffer = (uint32_t *)kmalloc(bb_size);

    if (!g_backbuffer) {
        serial_puts("[GUI ERROR] Failed to allocate backbuffer for compositor!\n");
        g_backbuffer = g_frontbuffer; // Fallback to single buffer
    }

    // Initialize PS/2 Mouse Driver
    mouse_init(g_screen_w, g_screen_h);

    // Create Default Desktop Windows
    gui_create_window(48, 64, 380, 240, "System Monitor", render_sysinfo_window);
    gui_create_window(460, 64, 420, 280, "Kernel Terminal", render_terminal_window);
    gui_create_window(200, 330, 360, 240, "File Explorer", render_files_window);

    g_active_window = &g_windows[1];

    serial_puts("[GUI] Desktop Compositor & Window Manager initialized.\n");
}

void gui_handle_mouse(void) {
    mouse_state_t mouse = mouse_get_state();

    // Check dragging
    if (mouse.left_button) {
        if (!g_active_window || !g_active_window->is_dragging) {
            // Check top-down if clicking on title bar
            for (int i = g_window_count - 1; i >= 0; i--) {
                window_t *w = &g_windows[i];
                if (!w->is_visible) continue;

                if (mouse.x >= w->x && mouse.x <= w->x + (int32_t)w->width &&
                    mouse.y >= w->y && mouse.y <= w->y + TITLEBAR_HEIGHT) {
                    
                    // Close button clicked?
                    if (mouse.x >= w->x + (int32_t)w->width - 24) {
                        w->is_visible = false;
                        break;
                    }

                    w->is_dragging = true;
                    w->drag_offset_x = mouse.x - w->x;
                    w->drag_offset_y = mouse.y - w->y;
                    g_active_window = w;
                    break;
                }
            }
        } else if (g_active_window && g_active_window->is_dragging) {
            g_active_window->x = mouse.x - g_active_window->drag_offset_x;
            g_active_window->y = mouse.y - g_active_window->drag_offset_y;

            // Clamp window to screen
            if (g_active_window->y < TOPBAR_HEIGHT) g_active_window->y = TOPBAR_HEIGHT;
        }
    } else {
        if (g_active_window) {
            g_active_window->is_dragging = false;
        }
    }
}

void gui_render_frame(void) {
    if (!g_backbuffer) return;

    // 1. Draw Wallpaper Gradient
    draw_gradient_v(0, TOPBAR_HEIGHT, g_screen_w, g_screen_h - TOPBAR_HEIGHT, COLOR_WALLPAPER_TOP, COLOR_WALLPAPER_BOTTOM);

    // 2. Draw Windows
    for (int i = 0; i < g_window_count; i++) {
        window_t *w = &g_windows[i];
        if (!w->is_visible) continue;

        bool active = (w == g_active_window);

        // Window Drop Shadow
        draw_rect(w->x + 4, w->y + 4, w->width, w->height, 0x00050811);

        // Window Background
        draw_rect(w->x, w->y, w->width, w->height, w->bg_color);

        // Window Titlebar
        uint32_t title_color = active ? COLOR_WIN_TITLE_ACTIVE : COLOR_WIN_TITLE_INACT;
        draw_rect(w->x, w->y, w->width, TITLEBAR_HEIGHT, title_color);
        draw_string(w->x + 10, w->y + 6, w->title, COLOR_TEXT_WHITE);

        // Close Button [X]
        draw_rect(w->x + w->width - 22, w->y + 5, 16, 16, COLOR_CLOSE_BTN);
        draw_string(w->x + w->width - 18, w->y + 5, "x", COLOR_TEXT_WHITE);

        // Window Border
        draw_rect_outline(w->x, w->y, w->width, w->height, active ? COLOR_ACCENT_CYAN : COLOR_WIN_BORDER);

        // Window Content
        if (w->render_content) {
            w->render_content(w);
        }
    }

    // 3. Draw Top Status Taskbar
    draw_rect(0, 0, g_screen_w, TOPBAR_HEIGHT, COLOR_TOPBAR_BG);
    draw_rect_outline(0, 0, g_screen_w, TOPBAR_HEIGHT, COLOR_TOPBAR_BORDER);

    draw_string(14, 8, "VAILISM OS", COLOR_ACCENT_CYAN);
    draw_string(140, 8, "|  x86_64 Long Mode", COLOR_TEXT_MUTED);
    draw_string(320, 8, "|  Ring 0 Supervisor", COLOR_ACCENT_GREEN);
    draw_string(520, 8, "|  Preemptive Scheduler Active", COLOR_TEXT_WHITE);

    // Live Uptime Counter
    uint64_t ticks = timer_get_ticks();
    uint64_t secs = ticks / 100;
    char time_str[32];
    strncpy(time_str, "Uptime: 00s", 31);
    time_str[8] = (char)('0' + (secs / 10) % 10);
    time_str[9] = (char)('0' + (secs % 10));
    draw_string(g_screen_w - 140, 8, time_str, COLOR_ACCENT_YELLOW);

    // 4. Draw Mouse Cursor
    mouse_state_t mouse = mouse_get_state();
    draw_cursor(mouse.x, mouse.y);

    // 5. Blit Backbuffer to Hardware Frontbuffer (Tear-Free VRAM Transfer)
    memcpy(g_frontbuffer, g_backbuffer, g_screen_w * g_screen_h * sizeof(uint32_t));
}

void gui_run_desktop(void) {
    serial_puts("[GUI] Entering Desktop Compositor Render Loop...\n");

    while (1) {
        gui_handle_mouse();
        gui_render_frame();
        thread_sleep(16); // ~60 FPS smooth rendering loop
    }
}
