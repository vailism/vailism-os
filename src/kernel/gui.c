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

#define TOPBAR_HEIGHT    30
#define TITLEBAR_HEIGHT  28
#define TASKBAR_HEIGHT   34
#define CORNER_RADIUS    6

// ─── Modern Slate/Blue Color Palette ─────────────────────────────────────────

#define COLOR_WALLPAPER_TOP    0x00070D1A
#define COLOR_WALLPAPER_MID    0x000F1B33
#define COLOR_WALLPAPER_BOT    0x001A2744

#define COLOR_TOPBAR_BG        0x000A1120
#define COLOR_TOPBAR_ACCENT    0x00182848

#define COLOR_TASKBAR_BG       0x000C1425
#define COLOR_TASKBAR_BTN      0x001E3050
#define COLOR_TASKBAR_BTN_ACT  0x002563EB

#define COLOR_WIN_BG           0x00131C2E
#define COLOR_WIN_CONTENT_BG   0x00101826
#define COLOR_WIN_BORDER       0x002A3A52
#define COLOR_WIN_BORDER_ACT   0x003B82F6
#define COLOR_WIN_TITLE_ACTIVE 0x001D4ED8
#define COLOR_WIN_TITLE_INACT  0x001C2840
#define COLOR_WIN_TITLE_GRAD   0x002563EB

#define COLOR_CLOSE_BTN        0x00DC2626
#define COLOR_CLOSE_BTN_HOVER  0x00EF4444
#define COLOR_MIN_BTN          0x00F59E0B
#define COLOR_MAX_BTN          0x0022C55E

#define COLOR_TEXT_WHITE       0x00F1F5F9
#define COLOR_TEXT_DIM         0x00CBD5E1
#define COLOR_TEXT_MUTED       0x00708090
#define COLOR_ACCENT_CYAN      0x0038BDF8
#define COLOR_ACCENT_GREEN     0x004ADE80
#define COLOR_ACCENT_YELLOW    0x00FACC15
#define COLOR_ACCENT_ORANGE    0x00FB923C
#define COLOR_ACCENT_PURPLE    0x00A78BFA

// ─── 12x18 Arrow Cursor Bitmap ───────────────────────────────────────────────

static const uint16_t cursor_bitmap[18] = {
    0b110000000000,
    0b111000000000,
    0b111100000000,
    0b111110000000,
    0b111111000000,
    0b111111100000,
    0b111111110000,
    0b111111111000,
    0b111111111100,
    0b111111111110,
    0b111111110000,
    0b111011110000,
    0b110001111000,
    0b100000111100,
    0b000000011110,
    0b000000001111,
    0b000000000110,
    0b000000000000
};

static const uint16_t cursor_outline[18] = {
    0b100000000000,
    0b100100000000,
    0b100010000000,
    0b100001000000,
    0b100000100000,
    0b100000010000,
    0b100000001000,
    0b100000000100,
    0b100000000010,
    0b100000000001,
    0b100000001110,
    0b100100001000,
    0b101110000100,
    0b011000100010,
    0b000000010001,
    0b000000001000,
    0b000000010001,
    0b000000001100
};

// ─── Drawing Primitives ──────────────────────────────────────────────────────

static void put_pixel(int32_t x, int32_t y, uint32_t color) {
    if (x < 0 || x >= (int32_t)g_screen_w || y < 0 || y >= (int32_t)g_screen_h) return;
    g_backbuffer[y * g_pitch_pixels + x] = color;
}

static void draw_rect(int32_t x, int32_t y, uint32_t w, uint32_t h, uint32_t color) {
    int32_t x0 = x < 0 ? 0 : x;
    int32_t y0 = y < 0 ? 0 : y;
    int32_t x1 = (int32_t)(x + w) > (int32_t)g_screen_w ? (int32_t)g_screen_w : (int32_t)(x + w);
    int32_t y1 = (int32_t)(y + h) > (int32_t)g_screen_h ? (int32_t)g_screen_h : (int32_t)(y + h);
    for (int32_t py = y0; py < y1; py++) {
        for (int32_t px = x0; px < x1; px++) {
            g_backbuffer[py * g_pitch_pixels + px] = color;
        }
    }
}

static void draw_hline(int32_t x, int32_t y, uint32_t w, uint32_t color) {
    if (y < 0 || y >= (int32_t)g_screen_h) return;
    int32_t x0 = x < 0 ? 0 : x;
    int32_t x1 = (int32_t)(x + w) > (int32_t)g_screen_w ? (int32_t)g_screen_w : (int32_t)(x + w);
    for (int32_t px = x0; px < x1; px++) {
        g_backbuffer[y * g_pitch_pixels + px] = color;
    }
}

static void draw_vline(int32_t x, int32_t y, uint32_t h, uint32_t color) {
    if (x < 0 || x >= (int32_t)g_screen_w) return;
    int32_t y0 = y < 0 ? 0 : y;
    int32_t y1 = (int32_t)(y + h) > (int32_t)g_screen_h ? (int32_t)g_screen_h : (int32_t)(y + h);
    for (int32_t py = y0; py < y1; py++) {
        g_backbuffer[py * g_pitch_pixels + x] = color;
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

static void draw_char(int32_t px, int32_t py, char c, uint32_t fg) {
    if (c < 32 || c > 126) c = '?';
    const uint8_t *glyph = font8x16_basic[c - 32];
    for (uint32_t row = 0; row < FONT_HEIGHT; row++) {
        uint8_t bits = glyph[row];
        for (uint32_t col = 0; col < FONT_WIDTH; col++) {
            if ((bits >> (7 - col)) & 1) {
                put_pixel(px + col, py + row, fg);
            }
        }
    }
}

static void draw_string(int32_t x, int32_t y, const char *str, uint32_t fg) {
    if (!str) return;
    int32_t cx = x;
    for (size_t i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') {
            cx = x;
            y += FONT_HEIGHT + 2;
            continue;
        }
        draw_char(cx, y, str[i], fg);
        cx += FONT_WIDTH;
    }
}

static void draw_cursor(int32_t mx, int32_t my) {
    // Black outline first (contrast on any background)
    for (int y = 0; y < 18; y++) {
        uint16_t row = cursor_outline[y];
        for (int x = 0; x < 12; x++) {
            if ((row >> (11 - x)) & 1) {
                put_pixel(mx + x, my + y, 0x00000000);
            }
        }
    }
    // White fill
    for (int y = 0; y < 18; y++) {
        uint16_t row = cursor_bitmap[y];
        for (int x = 0; x < 12; x++) {
            if ((row >> (11 - x)) & 1) {
                put_pixel(mx + x, my + y, 0x00FFFFFF);
            }
        }
    }
}

// Traffic light button (macOS-style)
static void draw_circle_btn(int32_t cx, int32_t cy, uint32_t color) {
    for (int dy = -5; dy <= 5; dy++) {
        for (int dx = -5; dx <= 5; dx++) {
            if (dx * dx + dy * dy <= 25) {
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

    draw_string(cx, cy, "SYSTEM MONITOR", COLOR_ACCENT_CYAN);
    cy += 24;

    // Separator line
    draw_hline(cx, cy, win->width - 32, COLOR_WIN_BORDER);
    cy += 10;

    draw_string(cx, cy, "CPU", COLOR_TEXT_MUTED);
    draw_string(cx + 80, cy, "x86_64 Long Mode  (Ring 0 Supervisor)", COLOR_TEXT_WHITE);
    cy += 20;

    draw_string(cx, cy, "Kernel", COLOR_TEXT_MUTED);
    draw_string(cx + 80, cy, "Vailism Monolithic Core", COLOR_TEXT_WHITE);
    cy += 20;

    draw_string(cx, cy, "Sched", COLOR_TEXT_MUTED);
    draw_string(cx + 80, cy, "Preemptive Round-Robin @ 100 Hz", COLOR_TEXT_WHITE);
    cy += 20;

    draw_string(cx, cy, "VFS", COLOR_TEXT_MUTED);
    draw_string(cx + 80, cy, "ATA PIO + RamFS mounted at /", COLOR_TEXT_WHITE);
    cy += 28;

    // Real dynamic RAM stats
    uint64_t total_mb = pmm_get_total_memory() / (1024 * 1024);
    uint64_t used_mb  = pmm_get_used_memory() / (1024 * 1024);
    uint64_t free_mb  = pmm_get_free_memory() / (1024 * 1024);

    draw_string(cx, cy, "PHYSICAL MEMORY", COLOR_ACCENT_CYAN);
    cy += 20;

    ksnprintf(buf, sizeof(buf), "Total: %u MB   Used: %u MB   Free: %u MB",
              (uint64_t)total_mb, (uint64_t)used_mb, (uint64_t)free_mb);
    draw_string(cx, cy, buf, COLOR_ACCENT_GREEN);
    cy += 22;

    // Proportional memory usage bar
    uint32_t bar_w = win->width - 36;
    uint32_t used_w = (total_mb > 0) ? (uint32_t)((used_mb * bar_w) / total_mb) : 0;
    if (used_w < 4) used_w = 4;

    draw_rect(cx, cy, bar_w, 16, COLOR_WIN_CONTENT_BG);
    // Used portion gradient
    for (uint32_t px = 0; px < used_w; px++) {
        uint32_t c = blend_color(COLOR_ACCENT_CYAN, COLOR_ACCENT_PURPLE, px, bar_w);
        for (uint32_t py = 0; py < 16; py++) {
            put_pixel(cx + px, cy + py, c);
        }
    }
    draw_rect_outline(cx, cy, bar_w, 16, COLOR_WIN_BORDER);
    cy += 24;

    // Uptime
    uint64_t ticks = timer_get_ticks();
    uint64_t secs = ticks / 100;
    uint64_t mins = secs / 60;
    ksnprintf(buf, sizeof(buf), "Uptime: %um %us", (uint64_t)mins, (uint64_t)(secs % 60));
    draw_string(cx, cy, buf, COLOR_ACCENT_YELLOW);
}

static void render_terminal_window(window_t *win) {
    int32_t cx = win->x + 14;
    int32_t cy = win->y + TITLEBAR_HEIGHT + 12;

    // Terminal content area background
    draw_rect(win->x + 4, win->y + TITLEBAR_HEIGHT + 4, win->width - 8, win->height - TITLEBAR_HEIGHT - 8, COLOR_WIN_CONTENT_BG);

    cx = win->x + 14;
    cy = win->y + TITLEBAR_HEIGHT + 12;

    draw_string(cx, cy, "vailism-os> uname", COLOR_ACCENT_CYAN);
    cy += 18;
    draw_string(cx, cy, "Vailism OS 0.7.0-desktop (x86_64 Long Mode)", COLOR_TEXT_WHITE);
    cy += 22;

    draw_string(cx, cy, "vailism-os> cat /etc/os-release", COLOR_ACCENT_CYAN);
    cy += 18;
    draw_string(cx, cy, "NAME=\"Vailism OS\"", COLOR_ACCENT_YELLOW);
    cy += 16;
    draw_string(cx, cy, "VERSION=\"0.7.0-desktop\"", COLOR_ACCENT_YELLOW);
    cy += 16;
    draw_string(cx, cy, "ARCH=\"x86_64\"", COLOR_ACCENT_YELLOW);
    cy += 22;

    draw_string(cx, cy, "vailism-os> ls /etc", COLOR_ACCENT_CYAN);
    cy += 18;
    draw_string(cx, cy, "[DIR]  os-release  hostname", COLOR_TEXT_DIM);
    cy += 22;

    // Blinking cursor simulation (alternate each second)
    uint64_t phase = (timer_get_ticks() / 50) % 2;
    draw_string(cx, cy, "vailism-os>", COLOR_ACCENT_CYAN);
    if (phase == 0) {
        draw_rect(cx + 96, cy + 2, 8, 14, COLOR_ACCENT_GREEN);
    }
}

static void render_files_window(window_t *win) {
    int32_t cx = win->x + 16;
    int32_t cy = win->y + TITLEBAR_HEIGHT + 16;

    draw_string(cx, cy, "VFS EXPLORER  /", COLOR_ACCENT_CYAN);
    cy += 22;
    draw_hline(cx, cy, win->width - 32, COLOR_WIN_BORDER);
    cy += 10;

    // Directory listing with icons
    const char *items[][3] = {
        {"DIR",  "/boot",  ""},
        {"DIR",  "/dev",   ""},
        {"DIR",  "/etc",   "2 files"},
        {"FILE", "  os-release", "147 bytes"},
        {"FILE", "  hostname",   "12 bytes"},
        {"DIR",  "/home",  "2 files"},
        {"FILE", "  welcome.txt", "73 bytes"},
        {"FILE", "  demo.txt",    "49 bytes"},
    };

    for (int i = 0; i < 8; i++) {
        bool is_dir = (items[i][0][0] == 'D');
        uint32_t name_color = is_dir ? COLOR_ACCENT_CYAN : COLOR_TEXT_DIM;
        uint32_t type_color = is_dir ? COLOR_ACCENT_GREEN : COLOR_TEXT_MUTED;

        draw_string(cx, cy, is_dir ? "[D]" : "   ", type_color);
        draw_string(cx + 32, cy, items[i][1], name_color);
        if (items[i][2][0] != '\0') {
            draw_string(cx + 200, cy, items[i][2], COLOR_TEXT_MUTED);
        }
        cy += 18;
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

    // Allocate backbuffer for double-buffering (tear-free compositing)
    size_t bb_size = g_screen_w * g_screen_h * sizeof(uint32_t);
    g_backbuffer = (uint32_t *)kmalloc(bb_size);
    if (!g_backbuffer) {
        serial_puts("[GUI ERROR] Failed to allocate backbuffer for compositor!\n");
        g_backbuffer = g_frontbuffer;
    }

    // Initialize PS/2 Mouse Driver
    mouse_init(g_screen_w, g_screen_h);

    // Create Default Desktop Windows
    gui_create_window(40,  50,  440, 310, "System Monitor",  render_sysinfo_window);
    gui_create_window(500, 50,  460, 340, "Kernel Terminal", render_terminal_window);
    gui_create_window(160, 390, 400, 280, "File Explorer",   render_files_window);

    g_active_window = &g_windows[0];

    serial_puts("[GUI] Desktop Compositor & Window Manager initialized.\n");
}

void gui_handle_mouse(void) {
    mouse_state_t mouse = mouse_get_state();

    if (mouse.left_button) {
        if (!g_active_window || !g_active_window->is_dragging) {
            for (int i = g_window_count - 1; i >= 0; i--) {
                window_t *w = &g_windows[i];
                if (!w->is_visible) continue;

                // Check click within window area
                if (mouse.x >= w->x && mouse.x <= w->x + (int32_t)w->width &&
                    mouse.y >= w->y && mouse.y <= w->y + (int32_t)w->height) {

                    // Check titlebar region
                    if (mouse.y <= w->y + TITLEBAR_HEIGHT) {
                        // Close button (red dot)
                        if (mouse.x >= w->x + 8 && mouse.x <= w->x + 22) {
                            w->is_visible = false;
                            if (g_active_window == w) g_active_window = NULL;
                            break;
                        }
                        // Start drag
                        w->is_dragging = true;
                        w->drag_offset_x = mouse.x - w->x;
                        w->drag_offset_y = mouse.y - w->y;
                    }

                    // Activate this window (bring to focus)
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
        // Release drag on any window
        for (int i = 0; i < g_window_count; i++) {
            g_windows[i].is_dragging = false;
        }
    }
}

// ─── Compositor: Render One Full Frame ───────────────────────────────────────

void gui_render_frame(void) {
    if (!g_backbuffer) return;

    // 1. Desktop Wallpaper — three-stop vertical gradient
    uint32_t half = (g_screen_h - TOPBAR_HEIGHT - TASKBAR_HEIGHT) / 2;
    draw_gradient_v(0, TOPBAR_HEIGHT, g_screen_w, half, COLOR_WALLPAPER_TOP, COLOR_WALLPAPER_MID);
    draw_gradient_v(0, TOPBAR_HEIGHT + half, g_screen_w, half, COLOR_WALLPAPER_MID, COLOR_WALLPAPER_BOT);

    // 2. Render Windows (bottom to top stacking order)
    for (int i = 0; i < g_window_count; i++) {
        window_t *w = &g_windows[i];
        if (!w->is_visible) continue;
        bool active = (w == g_active_window);

        // Drop shadow (layered for soft appearance)
        draw_rect(w->x + 6, w->y + 6, w->width, w->height, 0x00030812);
        draw_rect(w->x + 3, w->y + 3, w->width, w->height, 0x00060C18);

        // Window body
        draw_rect(w->x, w->y, w->width, w->height, w->bg_color);

        // Titlebar gradient
        uint32_t title_top = active ? COLOR_WIN_TITLE_ACTIVE : COLOR_WIN_TITLE_INACT;
        uint32_t title_bot = active ? COLOR_WIN_TITLE_GRAD : COLOR_WIN_TITLE_INACT;
        draw_gradient_v(w->x, w->y, w->width, TITLEBAR_HEIGHT, title_top, title_bot);

        // Traffic-light buttons (macOS style)
        draw_circle_btn(w->x + 16, w->y + TITLEBAR_HEIGHT / 2, COLOR_CLOSE_BTN);
        draw_circle_btn(w->x + 34, w->y + TITLEBAR_HEIGHT / 2, COLOR_MIN_BTN);
        draw_circle_btn(w->x + 52, w->y + TITLEBAR_HEIGHT / 2, COLOR_MAX_BTN);

        // Title text (centered-ish)
        int title_len = (int)strlen(w->title);
        int title_px = w->x + (int)(w->width / 2) - (title_len * FONT_WIDTH / 2);
        draw_string(title_px, w->y + 7, w->title, COLOR_TEXT_WHITE);

        // Window border
        draw_rect_outline(w->x, w->y, w->width, w->height, active ? COLOR_WIN_BORDER_ACT : COLOR_WIN_BORDER);

        // Content area subtle inner border
        draw_rect_outline(w->x + 1, w->y + TITLEBAR_HEIGHT, w->width - 2, w->height - TITLEBAR_HEIGHT - 1, 0x000A1020);

        // Render window content
        if (w->render_content) {
            w->render_content(w);
        }
    }

    // 3. Top Status Bar
    draw_rect(0, 0, g_screen_w, TOPBAR_HEIGHT, COLOR_TOPBAR_BG);
    draw_hline(0, TOPBAR_HEIGHT - 1, g_screen_w, COLOR_TOPBAR_ACCENT);

    // Left: OS brand
    draw_string(14, 7, "VAILISM OS", COLOR_ACCENT_CYAN);
    draw_string(112, 7, "|", COLOR_TEXT_MUTED);

    // Center: status indicators
    draw_string(130, 7, "x86_64", COLOR_TEXT_DIM);
    draw_string(188, 7, "|", COLOR_TEXT_MUTED);
    draw_string(204, 7, "Ring 0", COLOR_ACCENT_GREEN);
    draw_string(256, 7, "|", COLOR_TEXT_MUTED);
    draw_string(272, 7, "Scheduler Active", COLOR_TEXT_DIM);

    // Right: live clock / uptime
    {
        uint64_t ticks = timer_get_ticks();
        uint64_t secs = ticks / 100;
        uint64_t hrs  = secs / 3600;
        uint64_t mins = (secs % 3600) / 60;
        uint64_t s    = secs % 60;
        char time_buf[32];
        ksnprintf(time_buf, sizeof(time_buf), "%u:%02u:%02u", (uint64_t)hrs, (uint64_t)mins, (uint64_t)s);
        // Format mm:ss with leading zeros
        int len = (int)strlen(time_buf);
        draw_string(g_screen_w - (len * FONT_WIDTH) - 16, 7, time_buf, COLOR_ACCENT_YELLOW);
    }

    // 4. Bottom Taskbar
    int32_t tb_y = g_screen_h - TASKBAR_HEIGHT;
    draw_rect(0, tb_y, g_screen_w, TASKBAR_HEIGHT, COLOR_TASKBAR_BG);
    draw_hline(0, tb_y, g_screen_w, COLOR_TOPBAR_ACCENT);

    // Window tabs in taskbar
    int32_t tab_x = 12;
    for (int i = 0; i < g_window_count; i++) {
        window_t *w = &g_windows[i];
        if (!w->is_visible) continue;

        bool active = (w == g_active_window);
        uint32_t btn_color = active ? COLOR_TASKBAR_BTN_ACT : COLOR_TASKBAR_BTN;
        int title_len = (int)strlen(w->title);
        uint32_t tab_w = (uint32_t)(title_len * FONT_WIDTH + 20);

        draw_rect(tab_x, tb_y + 5, tab_w, 24, btn_color);
        draw_rect_outline(tab_x, tb_y + 5, tab_w, 24, active ? COLOR_WIN_BORDER_ACT : COLOR_WIN_BORDER);
        draw_string(tab_x + 10, tb_y + 9, w->title, COLOR_TEXT_WHITE);
        tab_x += (int32_t)tab_w + 8;
    }

    // Right side of taskbar: RAM usage
    {
        uint64_t used_mb = pmm_get_used_memory() / (1024 * 1024);
        uint64_t total_mb = pmm_get_total_memory() / (1024 * 1024);
        char ram_buf[32];
        ksnprintf(ram_buf, sizeof(ram_buf), "RAM: %u/%u MB", (uint64_t)used_mb, (uint64_t)total_mb);
        int len = (int)strlen(ram_buf);
        draw_string(g_screen_w - (len * FONT_WIDTH) - 16, tb_y + 9, ram_buf, COLOR_ACCENT_GREEN);
    }

    // 5. Mouse Cursor (always on top)
    mouse_state_t mouse = mouse_get_state();
    draw_cursor(mouse.x, mouse.y);

    // 6. Blit to hardware VRAM
    memcpy(g_frontbuffer, g_backbuffer, g_screen_w * g_screen_h * sizeof(uint32_t));
}

void gui_run_desktop(void) {
    serial_puts("[GUI] Entering Desktop Compositor Render Loop...\n");

    while (1) {
        gui_handle_mouse();
        gui_render_frame();
        thread_sleep(16); // ~60 FPS
    }
}
