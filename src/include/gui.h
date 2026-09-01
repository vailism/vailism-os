#ifndef GUI_H
#define GUI_H

#include "types.h"
#include "framebuffer.h"

#define MAX_WINDOWS 8

typedef struct window {
    int32_t     x;
    int32_t     y;
    uint32_t    width;
    uint32_t    height;
    char        title[64];
    bool        is_visible;
    bool        is_active;
    bool        is_dragging;
    int32_t     drag_offset_x;
    int32_t     drag_offset_y;
    uint32_t    bg_color;
    void      (*render_content)(struct window *win);
} window_t;

/**
 * Initialize Desktop GUI, Window Manager, and Offscreen Backbuffer.
 */
void gui_init(struct limine_framebuffer *fb);

/**
 * Render one complete frame (Desktop + Windows + Taskbar + Mouse Cursor) and blit to screen.
 */
void gui_render_frame(void);

/**
 * Handle mouse update and click events in the window manager.
 */
void gui_handle_mouse(void);

/**
 * Create a new window managed by the desktop.
 */
window_t *gui_create_window(int32_t x, int32_t y, uint32_t w, uint32_t h, const char *title, void (*render)(window_t *));

/**
 * Run GUI desktop main loop.
 */
void gui_run_desktop(void);

#endif // GUI_H
