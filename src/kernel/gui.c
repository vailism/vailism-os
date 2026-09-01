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
static uint32_t  g_screen_w = 1024;
static uint32_t  g_screen_h = 768;
static uint32_t  g_pitch_pixels = 1024;

static window_t  g_windows[MAX_WINDOWS];
static int       g_window_count = 0;
static window_t *g_active_window = NULL;

#define TOPBAR_HEIGHT    36
#define TITLEBAR_HEIGHT  32
#define TASKBAR_HEIGHT   40

// ─── High-Contrast Sleek Color Palette ───────────────────────────────────────

#define COLOR_WALLPAPER_TOP    0x000F172A // Deep slate blue
#define COLOR_WALLPAPER_BOT    0x001E293B // Dark indigo slate

#define COLOR_TOPBAR_BG        0x00020617 // Near black
#define COLOR_TOPBAR_BORDER    0x00334155 // Slate outline

#define COLOR_TASKBAR_BG       0x00020617
#define COLOR_TASKBAR_BTN      0x001E293B
#define COLOR_TASKBAR_BTN_ACT  0x002563EB

#define COLOR_WIN_BG           0x000F172A
#define COLOR_WIN_CONTENT_BG   0x00020617
#define COLOR_WIN_BORDER       0x00334155
#define COLOR_WIN_BORDER_ACT   0x003B82F6
#define COLOR_WIN_TITLE_ACTIVE 0x001D4ED8
#define COLOR_WIN_TITLE_INACT  0x001E293B

#define COLOR_CLOSE_BTN        0x00EF4444
#define COLOR_MIN_BTN          0x00F59E0B
#define COLOR_MAX_BTN          0x0010B981

#define COLOR_TEXT_WHITE       0x00FFFFFF // Pure Crisp White
#define COLOR_TEXT_CYAN        0x0038BDF8 // Electric Cyan
#define COLOR_TEXT_GREEN       0x004ADE80 // Vivid Emerald
#define COLOR_TEXT_YELLOW      0x00FDE047 // Bright Amber
#define COLOR_TEXT_MUTED       0x0094A3B8 // Soft Gray

// ─── 14x22 Large High-Visibility Cursor ───────────────────────────────────────

static const uint32_t cursor_pixels[22][14] = {
    {1,1,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0,0,0,0,0,0,0},
    {1,2,2,1,0,0,0,0,0,0,0,0,0,0},
    {1,2,2,2,1,0,0,0,0,0,0,0,0,0},
    {1,2,2,2,2,1,0,0,0,0,0,0,0,0},
    {1,2,2,2,2,2,1,0,0,0,0,0,0,0},
    {1,2,2,2,2,2,2,1,0,0,0,0,0,0},
    {1,2,2,2,2,2,2,2,1,0,0,0,0,0},
    {1,2,2,2,2,2,2,2,2,1,0,0,0,0},
    {1,2,2,2,2,2,2,2,2,2,1,0,0,0},
    {1,2,2,2,2,2,2,2,2,2,2,1,0,0},
    {1,2,2,2,2,2,2,1,1,1,1,1,0,0},
    {1,2,2,2,1,2,2,1,0,0,0,0,0,0},
    {1,2,2,1,0,1,2,2,1,0,0,0,0,0},
    {1,2,1,0,0,1,2,2,1,0,0,0,0,0},
    {1,1,0,0,0,0,1,2,2,1,0,0,0,0},
    {0,0,0,0,0,0,1,2,2,1,0,0,0,0},
    {0,0,0,0,0,0,0,1,1,0,0,0,0,0}
};

// ─── Drawing Primitives ──────────────────────────────────────────────────────

static void put_pixel(int32_t x, int32_t y, uint32_t color) {
    if (x < 0 || x >= (int32_t)g_screen_w || y < 0 || y >= (int32_t)g_screen_h) return;
    g_backbuffer[y * g_screen_w + x] = color;
}

static void draw_rect(int32_t x, int32_t y, uint32_t w, uint32_t h, uint32_t color) {
    int32_t x0 = x < 0 ? 0 : x;
    int32_t y0 = y < 0 ? 0 : y;
    int32_t x1 = (int32_t)(x + w) > (int32_t)g_screen_w ? (int32_t)g_screen_w : (int32_t)(x + w);
    int32_t y1 = (int32_t)(y + h) > (int32_t)g_screen_h ? (int32_t)g_screen_h : (int32_t)(y + h);
    for (int32_t py = y0; py < y1; py++) {
        for (int32_t px = x0; px < x1; px++) {
            g_backbuffer[py * g_screen_w + px] = color;
        }
    }
}

static void draw_hline(int32_t x, int32_t y, uint32_t w, uint32_t color) {
    if (y < 0 || y >= (int32_t)g_screen_h) return;
    int32_t x0 = x < 0 ? 0 : x;
    int32_t x1 = (int32_t)(x + w) > (int32_t)g_screen_w ? (int32_t)g_screen_w : (int32_t)(x + w);
    for (int32_t px = x0; px < x1; px++) {
        g_backbuffer[y * g_screen_w + px] = color;
    }
}

static void draw_vline(int32_t x, int32_t y, uint32_t h, uint32_t color) {
    if (x < 0 || x >= (int32_t)g_screen_w) return;
    int32_t y0 = y < 0 ? 0 : y;
    int32_t y1 = (int32_t)(y + h) > (int32_t)g_screen_h ? (int32_t)g_screen_h : (int32_t)(y + h);
    for (int32_t py = y0; py < y1; py++) {
        g_backbuffer[py * g_screen_w + x] = color;
    }
}

static void draw_rect_outline(int32_t x, int32_t y, uint32_t w, uint32_t h, uint32_t color) {
    draw_hline(x, y, w, color);
    draw_hline(x, y + h - 1, w, color);
    draw_vline(x, y, h, color);
    draw_vline(x + w - 1, y, h, color);
}

static uint32_t blend_color(uint32_t c1, uint32_t c2, uint32_t t, uint32_t range) {
    uint8_t r1 = (c1 >> 16) & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = c1 & 0xFF;
    uint8_t r2 = (c2 >> 16) & 0xFF, g2 = (c2 >> 8) & 0xFF, b2 = c2 & 0xFF;
    uint8_t r = (uint8_t)(r1 + ((int32_t)(r2 - r1) * (int32_t)t) / (int32_t)range);
    uint8_t g = (uint8_t)(g1 + ((int32_t)(g2 - g1) * (int32_t)t) / (int32_t)range);
    uint8_t b = (uint8_t)(b1 + ((int32_t)(b2 - b1) * (int32_t)t) / (int32_t)range);
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static void draw_gradient_v(int32_t x, int32_t y, uint32_t w, uint32_t h, uint32_t top, uint32_t bot) {
    for (uint32_t py = 0; py < h; py++) {
        uint32_t row_color = blend_color(top, bot, py, h);
        draw_hline(x, y + py, w, row_color);
    }
}

// Crisp Scaled Character Rendering
static void draw_char_scaled(int32_t px, int32_t py, char c, uint32_t fg, int scale) {
    if (c < 32 || c > 126) c = '?';
    const uint8_t *glyph = font8x16_basic[c - 32];
    for (uint32_t row = 0; row < FONT_HEIGHT; row++) {
        uint8_t bits = glyph[row];
        for (uint32_t col = 0; col < FONT_WIDTH; col++) {
            if ((bits >> (7 - col)) & 1) {
                if (scale == 1) {
                    put_pixel(px + col, py + row, fg);
                } else {
                    draw_rect(px + col * scale, py + row * scale, scale, scale, fg);
                }
            }
        }
    }
}

static void draw_string_scaled(int32_t x, int32_t y, const char *str, uint32_t fg, int scale) {
    if (!str) return;
    int32_t cx = x;
    int char_w = FONT_WIDTH * scale;
    int char_h = FONT_HEIGHT * scale;
    for (size_t i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') {
            cx = x;
            y += char_h + (2 * scale);
            continue;
        }
        draw_char_scaled(cx, y, str[i], fg, scale);
        cx += char_w;
    }
}


static void draw_cursor(int32_t mx, int32_t my) {
    for (int y = 0; y < 18; y++) {
        for (int x = 0; x < 14; x++) {
            uint32_t p = cursor_pixels[y][x];
            if (p == 1) {
                put_pixel(mx + x, my + y, 0x00000000); // Black outline
            } else if (p == 2) {
                put_pixel(mx + x, my + y, 0x00FFFFFF); // Pure white fill
            }
        }
    }
}

static void draw_circle_btn(int32_t cx, int32_t cy, uint32_t color) {
    for (int dy = -6; dy <= 6; dy++) {
        for (int dx = -6; dx <= 6; dx++) {
            if (dx * dx + dy * dy <= 36) {
                put_pixel(cx + dx, cy + dy, color);
            }
        }
    }
}

// ─── Built-in Window Content Renderers ───────────────────────────────────────

static void render_sysinfo_window(window_t *win) {
    int32_t cx = win->x + 16;
    int32_t cy = win->y + TITLEBAR_HEIGHT + 16;
    char buf[128];

    draw_string_scaled(cx, cy, "SYSTEM MONITOR", COLOR_TEXT_CYAN, 1);
    cy += 24;

    draw_hline(cx, cy, win->width - 32, COLOR_WIN_BORDER);
    cy += 12;

    draw_string_scaled(cx, cy, "CPU Specs:", COLOR_TEXT_MUTED, 1);
    draw_string_scaled(cx + 100, cy, "x86_64 Long Mode (Ring 0)", COLOR_TEXT_WHITE, 1);
    cy += 22;

    draw_string_scaled(cx, cy, "Kernel Core:", COLOR_TEXT_MUTED, 1);
    draw_string_scaled(cx + 100, cy, "Vailism Monolithic v0.7", COLOR_TEXT_WHITE, 1);
    cy += 22;

    draw_string_scaled(cx, cy, "Scheduler:", COLOR_TEXT_MUTED, 1);
    draw_string_scaled(cx + 100, cy, "Preemptive Round-Robin (100 Hz)", COLOR_TEXT_WHITE, 1);
    cy += 22;

    draw_string_scaled(cx, cy, "FileSystem:", COLOR_TEXT_MUTED, 1);
    draw_string_scaled(cx + 100, cy, "ATA/IDE PIO + RamFS (Mounted /)", COLOR_TEXT_WHITE, 1);
    cy += 30;

    // Real dynamic RAM stats
    uint64_t total_mb = pmm_get_total_memory() / (1024 * 1024);
    uint64_t used_mb  = pmm_get_used_memory() / (1024 * 1024);
    uint64_t free_mb  = pmm_get_free_memory() / (1024 * 1024);

    draw_string_scaled(cx, cy, "PHYSICAL MEMORY ALLOCATION", COLOR_TEXT_CYAN, 1);
    cy += 22;

    ksnprintf(buf, sizeof(buf), "Total: %u MB  |  Used: %u MB  |  Free: %u MB",
              (uint64_t)total_mb, (uint64_t)used_mb, (uint64_t)free_mb);
    draw_string_scaled(cx, cy, buf, COLOR_TEXT_GREEN, 1);
    cy += 24;

    // Large memory usage bar
    uint32_t bar_w = win->width - 36;
    uint32_t used_w = (total_mb > 0) ? (uint32_t)((used_mb * bar_w) / total_mb) : 0;
    if (used_w < 6) used_w = 6;

    draw_rect(cx, cy, bar_w, 20, COLOR_WIN_CONTENT_BG);
    draw_rect(cx, cy, used_w, 20, COLOR_TEXT_CYAN);
    draw_rect_outline(cx, cy, bar_w, 20, COLOR_WIN_BORDER_ACT);
    cy += 32;

    uint64_t ticks = timer_get_ticks();
    uint64_t secs = ticks / 100;
    uint64_t mins = secs / 60;
    ksnprintf(buf, sizeof(buf), "System Uptime: %um %us (%u ticks)", (uint64_t)mins, (uint64_t)(secs % 60), (uint64_t)ticks);
    draw_string_scaled(cx, cy, buf, COLOR_TEXT_YELLOW, 1);
}

static void render_terminal_window(window_t *win) {
    int32_t cx = win->x + 16;
    int32_t cy = win->y + TITLEBAR_HEIGHT + 14;

    draw_rect(win->x + 4, win->y + TITLEBAR_HEIGHT + 4, win->width - 8, win->height - TITLEBAR_HEIGHT - 8, COLOR_WIN_CONTENT_BG);

    draw_string_scaled(cx, cy, "vailism-os> uname -a", COLOR_TEXT_CYAN, 1);
    cy += 20;
    draw_string_scaled(cx, cy, "Vailism OS 0.7.0-desktop x86_64 Bare-Metal", COLOR_TEXT_WHITE, 1);
    cy += 26;

    draw_string_scaled(cx, cy, "vailism-os> cat /etc/os-release", COLOR_TEXT_CYAN, 1);
    cy += 20;
    draw_string_scaled(cx, cy, "NAME=\"Vailism OS\"", COLOR_TEXT_YELLOW, 1);
    cy += 18;
    draw_string_scaled(cx, cy, "VERSION=\"0.7.0-desktop\"", COLOR_TEXT_YELLOW, 1);
    cy += 18;
    draw_string_scaled(cx, cy, "ARCH=\"x86_64\"", COLOR_TEXT_YELLOW, 1);
    cy += 26;

    draw_string_scaled(cx, cy, "vailism-os> ls /home", COLOR_TEXT_CYAN, 1);
    cy += 20;
    draw_string_scaled(cx, cy, "[FILE] welcome.txt (73 bytes)", COLOR_TEXT_GREEN, 1);
    cy += 18;
    draw_string_scaled(cx, cy, "[FILE] demo.txt    (49 bytes)", COLOR_TEXT_GREEN, 1);
    cy += 26;

    // Blinking cursor simulation
    uint64_t phase = (timer_get_ticks() / 50) % 2;
    draw_string_scaled(cx, cy, "vailism-os> ", COLOR_TEXT_CYAN, 1);
    if (phase == 0) {
        draw_rect(cx + 96, cy, 10, 16, COLOR_TEXT_GREEN);
    }
}

static void render_files_window(window_t *win) {
    int32_t cx = win->x + 16;
    int32_t cy = win->y + TITLEBAR_HEIGHT + 16;

    draw_string_scaled(cx, cy, "VFS EXPLORER  /", COLOR_TEXT_CYAN, 1);
    cy += 24;
    draw_hline(cx, cy, win->width - 32, COLOR_WIN_BORDER);
    cy += 14;

    const char *items[][3] = {
        {"DIR",  "/boot",  "System Kernel"},
        {"DIR",  "/dev",   "Device Nodes"},
        {"DIR",  "/etc",   "Configuration"},
        {"FILE", "  os-release", "147 bytes"},
        {"FILE", "  hostname",   "12 bytes"},
        {"DIR",  "/home",  "User Space"},
        {"FILE", "  welcome.txt", "73 bytes"},
        {"FILE", "  demo.txt",    "49 bytes"},
    };

    for (int i = 0; i < 8; i++) {
        bool is_dir = (items[i][0][0] == 'D');
        uint32_t name_color = is_dir ? COLOR_TEXT_CYAN : COLOR_TEXT_WHITE;
        uint32_t type_color = is_dir ? COLOR_TEXT_GREEN : COLOR_TEXT_MUTED;

        draw_string_scaled(cx, cy, is_dir ? "[DIR] " : "      ", type_color, 1);
        draw_string_scaled(cx + 56, cy, items[i][1], name_color, 1);
        if (items[i][2][0] != '\0') {
            draw_string_scaled(cx + 220, cy, items[i][2], COLOR_TEXT_MUTED, 1);
        }
        cy += 22;
    }
}

// ─── Window Manager ──────────────────────────────────────────────────────────

window_t *gui_create_window(int32_t x, int32_t y, uint32_t w, uint32_t h, const char *title, void (*render)(window_t *)) {
    if (g_window_count >= MAX_WINDOWS) return NULL;

    window_t *win = &g_windows[g_window_count++];
    win->x = x;
    win->y = y;
    win->width = w;
    win->height = h;
    strncpy(win->title, title ? title : "Window", 63);
    win->is_visible = true;
    win->is_active = false;
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

    size_t bb_size = g_screen_w * g_screen_h * sizeof(uint32_t);
    g_backbuffer = (uint32_t *)kmalloc(bb_size);
    if (!g_backbuffer) {
        serial_puts("[GUI ERROR] Failed to allocate backbuffer for compositor!\n");
        g_backbuffer = g_frontbuffer;
    }

    mouse_init(g_screen_w, g_screen_h);

    // Create Generously Sized Desktop Windows
    gui_create_window(40,  60,  540, 380, "System Monitor",  render_sysinfo_window);
    gui_create_window(460, 80,  520, 420, "Kernel Terminal", render_terminal_window);
    gui_create_window(120, 320, 500, 360, "File Explorer",   render_files_window);

    g_active_window = &g_windows[1];

    serial_puts("[GUI] High-Contrast Desktop Compositor initialized.\n");
}

void gui_handle_mouse(void) {
    mouse_state_t mouse = mouse_get_state();

    if (mouse.left_button) {
        if (!g_active_window || !g_active_window->is_dragging) {
            for (int i = g_window_count - 1; i >= 0; i--) {
                window_t *w = &g_windows[i];
                if (!w->is_visible) continue;

                if (mouse.x >= w->x && mouse.x <= w->x + (int32_t)w->width &&
                    mouse.y >= w->y && mouse.y <= w->y + (int32_t)w->height) {

                    if (mouse.y <= w->y + TITLEBAR_HEIGHT) {
                        if (mouse.x >= w->x + 8 && mouse.x <= w->x + 28) {
                            w->is_visible = false;
                            if (g_active_window == w) g_active_window = NULL;
                            break;
                        }
                        w->is_dragging = true;
                        w->drag_offset_x = mouse.x - w->x;
                        w->drag_offset_y = mouse.y - w->y;
                    }

                    g_active_window = w;
                    break;
                }
            }
        } else if (g_active_window && g_active_window->is_dragging) {
            g_active_window->x = mouse.x - g_active_window->drag_offset_x;
            g_active_window->y = mouse.y - g_active_window->drag_offset_y;

            if (g_active_window->y < TOPBAR_HEIGHT) g_active_window->y = TOPBAR_HEIGHT;
            if (g_active_window->y + (int32_t)g_active_window->height > (int32_t)g_screen_h - TASKBAR_HEIGHT)
                g_active_window->y = g_screen_h - TASKBAR_HEIGHT - g_active_window->height;
        }
    } else {
        for (int i = 0; i < g_window_count; i++) {
            g_windows[i].is_dragging = false;
        }
    }
}

// ─── Compositor: Render One Full Frame ───────────────────────────────────────

void gui_render_frame(void) {
    if (!g_backbuffer) return;

    // 1. Solid High-Contrast Dark Slate Background
    draw_gradient_v(0, TOPBAR_HEIGHT, g_screen_w, g_screen_h - TOPBAR_HEIGHT - TASKBAR_HEIGHT, COLOR_WALLPAPER_TOP, COLOR_WALLPAPER_BOT);

    // 2. Render Windows
    for (int i = 0; i < g_window_count; i++) {
        window_t *w = &g_windows[i];
        if (!w->is_visible) continue;
        bool active = (w == g_active_window);

        // Drop shadow
        draw_rect(w->x + 8, w->y + 8, w->width, w->height, 0x00000000);

        // Window background
        draw_rect(w->x, w->y, w->width, w->height, w->bg_color);

        // Titlebar
        uint32_t title_color = active ? COLOR_WIN_TITLE_ACTIVE : COLOR_WIN_TITLE_INACT;
        draw_rect(w->x, w->y, w->width, TITLEBAR_HEIGHT, title_color);

        // Traffic Light Controls
        draw_circle_btn(w->x + 18, w->y + TITLEBAR_HEIGHT / 2, COLOR_CLOSE_BTN);
        draw_circle_btn(w->x + 38, w->y + TITLEBAR_HEIGHT / 2, COLOR_MIN_BTN);
        draw_circle_btn(w->x + 58, w->y + TITLEBAR_HEIGHT / 2, COLOR_MAX_BTN);

        // Window Title
        int title_len = (int)strlen(w->title);
        int title_px = w->x + (int)(w->width / 2) - (title_len * FONT_WIDTH / 2);
        draw_string_scaled(title_px, w->y + 8, w->title, COLOR_TEXT_WHITE, 1);

        // Window Outline
        draw_rect_outline(w->x, w->y, w->width, w->height, active ? COLOR_WIN_BORDER_ACT : COLOR_WIN_BORDER);
        if (active) {
            draw_rect_outline(w->x + 1, w->y + 1, w->width - 2, w->height - 2, COLOR_WIN_BORDER_ACT);
        }

        // Content area inner frame
        draw_rect_outline(w->x + 2, w->y + TITLEBAR_HEIGHT, w->width - 4, w->height - TITLEBAR_HEIGHT - 2, COLOR_WIN_BORDER);

        if (w->render_content) {
            w->render_content(w);
        }
    }

    // 3. Top Status Bar
    draw_rect(0, 0, g_screen_w, TOPBAR_HEIGHT, COLOR_TOPBAR_BG);
    draw_hline(0, TOPBAR_HEIGHT - 1, g_screen_w, COLOR_TOPBAR_BORDER);

    draw_string_scaled(16, 10, "VAILISM OS", COLOR_TEXT_CYAN, 1);
    draw_string_scaled(130, 10, "|  x86_64 Long Mode", COLOR_TEXT_WHITE, 1);
    draw_string_scaled(330, 10, "|  Ring 0 Supervisor", COLOR_TEXT_GREEN, 1);
    draw_string_scaled(550, 10, "|  Scheduler 100Hz", COLOR_TEXT_YELLOW, 1);

    uint64_t ticks = timer_get_ticks();
    uint64_t secs = ticks / 100;
    uint64_t hrs  = secs / 3600;
    uint64_t mins = (secs % 3600) / 60;
    uint64_t s    = secs % 60;
    char time_buf[32];
    ksnprintf(time_buf, sizeof(time_buf), "%u:%02u:%02u", (uint64_t)hrs, (uint64_t)mins, (uint64_t)s);
    int len = (int)strlen(time_buf);
    draw_string_scaled(g_screen_w - (len * FONT_WIDTH) - 20, 10, time_buf, COLOR_TEXT_YELLOW, 1);

    // 4. Bottom Taskbar
    int32_t tb_y = g_screen_h - TASKBAR_HEIGHT;
    draw_rect(0, tb_y, g_screen_w, TASKBAR_HEIGHT, COLOR_TASKBAR_BG);
    draw_hline(0, tb_y, g_screen_w, COLOR_TOPBAR_BORDER);

    int32_t tab_x = 16;
    for (int i = 0; i < g_window_count; i++) {
        window_t *w = &g_windows[i];
        if (!w->is_visible) continue;

        bool active = (w == g_active_window);
        uint32_t btn_color = active ? COLOR_TASKBAR_BTN_ACT : COLOR_TASKBAR_BTN;
        int title_len = (int)strlen(w->title);
        uint32_t tab_w = (uint32_t)(title_len * FONT_WIDTH + 24);

        draw_rect(tab_x, tb_y + 6, tab_w, 28, btn_color);
        draw_rect_outline(tab_x, tb_y + 6, tab_w, 28, active ? COLOR_WIN_BORDER_ACT : COLOR_WIN_BORDER);
        draw_string_scaled(tab_x + 12, tb_y + 12, w->title, COLOR_TEXT_WHITE, 1);
        tab_x += (int32_t)tab_w + 10;
    }

    // Right side RAM info
    {
        uint64_t used_mb = pmm_get_used_memory() / (1024 * 1024);
        uint64_t total_mb = pmm_get_total_memory() / (1024 * 1024);
        char ram_buf[32];
        ksnprintf(ram_buf, sizeof(ram_buf), "RAM: %u/%u MB", (uint64_t)used_mb, (uint64_t)total_mb);
        int len = (int)strlen(ram_buf);
        draw_string_scaled(g_screen_w - (len * FONT_WIDTH) - 20, tb_y + 12, ram_buf, COLOR_TEXT_GREEN, 1);
    }

    // 5. Large Cursor
    mouse_state_t mouse = mouse_get_state();
    draw_cursor(mouse.x, mouse.y);

    // 6. Safe Blit to Hardware Frontbuffer (Scanline by scanline)
    for (uint32_t y = 0; y < g_screen_h; y++) {
        memcpy(&g_frontbuffer[y * g_pitch_pixels], &g_backbuffer[y * g_screen_w], g_screen_w * sizeof(uint32_t));
    }
}

void gui_run_desktop(void) {
    serial_puts("[GUI] Entering Desktop Compositor Render Loop...\n");

    while (1) {
        gui_handle_mouse();
        gui_render_frame();
        thread_sleep(16);
    }
}
