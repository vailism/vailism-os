#include "../include/framebuffer.h"
#include "../include/font8x16.h"

static uint32_t *g_buffer = NULL;
static uint32_t g_pitch_pixels = 0; // Pitch divided by 4 (number of uint32_t per scanline)
static uint32_t g_width = 0;
static uint32_t g_height = 0;

static uint32_t g_cursor_x = 24; // Left margin padding
static uint32_t g_cursor_y = 24; // Top margin padding
static const uint32_t PADDING_X = 24;
static const uint32_t PADDING_Y = 24;

static uint32_t g_fg_color = FB_COLOR_WHITE;
static uint32_t g_bg_color = FB_COLOR_BG;

void fb_init(struct limine_framebuffer *fb) {
    if (!fb) return;
    g_buffer = (uint32_t *)fb->address;
    g_pitch_pixels = (uint32_t)(fb->pitch / 4);
    g_width = (uint32_t)fb->width;
    g_height = (uint32_t)fb->height;

    g_cursor_x = PADDING_X;
    g_cursor_y = PADDING_Y;

    fb_clear(FB_COLOR_BG);
}

void fb_putpixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!g_buffer || x >= g_width || y >= g_height) return;
    g_buffer[y * g_pitch_pixels + x] = color;
}

void fb_clear(uint32_t color) {
    if (!g_buffer) return;
    for (uint32_t y = 0; y < g_height; y++) {
        for (uint32_t x = 0; x < g_width; x++) {
            g_buffer[y * g_pitch_pixels + x] = color;
        }
    }
    g_cursor_x = PADDING_X;
    g_cursor_y = PADDING_Y;
}

void fb_set_color(uint32_t fg, uint32_t bg) {
    g_fg_color = fg;
    g_bg_color = bg;
}

static void fb_scroll(void) {
    uint32_t row_height = FONT_HEIGHT + 4;
    // Copy pixels up by one line of text
    for (uint32_t y = PADDING_Y; y < g_height - row_height - PADDING_Y; y++) {
        for (uint32_t x = PADDING_X; x < g_width - PADDING_X; x++) {
            g_buffer[y * g_pitch_pixels + x] = g_buffer[(y + row_height) * g_pitch_pixels + x];
        }
    }
    // Clear the bottom line
    for (uint32_t y = g_height - row_height - PADDING_Y; y < g_height - PADDING_Y; y++) {
        for (uint32_t x = PADDING_X; x < g_width - PADDING_X; x++) {
            g_buffer[y * g_pitch_pixels + x] = g_bg_color;
        }
    }
}

static void draw_char_at(char c, uint32_t px, uint32_t py, uint32_t fg, uint32_t bg) {
    if (c < 32 || c > 126) {
        c = '?';
    }
    const uint8_t *glyph = font8x16_basic[c - 32];

    for (uint32_t row = 0; row < FONT_HEIGHT; row++) {
        uint8_t bits = glyph[row];
        for (uint32_t col = 0; col < FONT_WIDTH; col++) {
            // Check bit from MSB to LSB (bit 7 to bit 0)
            if ((bits >> (7 - col)) & 1) {
                fb_putpixel(px + col, py + row, fg);
            } else {
                fb_putpixel(px + col, py + row, bg);
            }
        }
    }
}

void fb_putchar(char c) {
    if (!g_buffer) return;

    if (c == '\n') {
        g_cursor_x = PADDING_X;
        g_cursor_y += FONT_HEIGHT + 4;
        if (g_cursor_y + FONT_HEIGHT + PADDING_Y >= g_height) {
            fb_scroll();
            g_cursor_y -= (FONT_HEIGHT + 4);
        }
        return;
    }

    if (c == '\r') {
        g_cursor_x = PADDING_X;
        return;
    }

    if (c == '\b') {
        if (g_cursor_x >= PADDING_X + FONT_WIDTH) {
            g_cursor_x -= FONT_WIDTH;
            draw_char_at(' ', g_cursor_x, g_cursor_y, g_bg_color, g_bg_color);
        }
        return;
    }

    if (c == '\t') {
        for (int i = 0; i < 4; i++) {
            fb_putchar(' ');
        }
        return;
    }

    // Line wrap if exceeding screen width
    if (g_cursor_x + FONT_WIDTH + PADDING_X >= g_width) {
        fb_putchar('\n');
    }

    draw_char_at(c, g_cursor_x, g_cursor_y, g_fg_color, g_bg_color);
    g_cursor_x += FONT_WIDTH;
}

void fb_puts(const char *str) {
    if (!str) return;
    for (size_t i = 0; str[i] != '\0'; i++) {
        fb_putchar(str[i]);
    }
}
