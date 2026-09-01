#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "types.h"
#include "limine.h"

// Sleek 32-bit ARGB/XRGB color palette
#define FB_COLOR_BG       0x000F172A // Slate dark background (modern terminal feel)
#define FB_COLOR_WHITE    0x00F8FAFC
#define FB_COLOR_CYAN     0x0038BDF8
#define FB_COLOR_GREEN    0x004ADE80
#define FB_COLOR_YELLOW   0x00FDE047
#define FB_COLOR_RED      0x00F87171
#define FB_COLOR_PURPLE   0x00C084FC
#define FB_COLOR_MUTED    0x0094A3B8

/**
 * Initialize the framebuffer subsystem using Limine's provided buffer.
 */
void fb_init(struct limine_framebuffer *fb);

/**
 * Draw a single pixel at (x, y) with 32-bit color 0x00RRGGBB.
 */
void fb_putpixel(uint32_t x, uint32_t y, uint32_t color);

/**
 * Clear the screen with a specific background color.
 */
void fb_clear(uint32_t color);

/**
 * Draw a single character using the embedded 8x16 font.
 */
void fb_putchar(char c);

/**
 * Print a null-terminated string to the framebuffer terminal.
 */
void fb_puts(const char *str);

/**
 * Set the foreground and background colors for subsequent text output.
 */
void fb_set_color(uint32_t fg, uint32_t bg);

/**
 * Get the framebuffer width in pixels.
 */
uint32_t fb_get_width(void);

/**
 * Get the framebuffer height in pixels.
 */
uint32_t fb_get_height(void);

#endif // FRAMEBUFFER_H
