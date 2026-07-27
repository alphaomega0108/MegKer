/* gui/gui.c
 * Minimal windowing compositor: a handful of draggable, colored
 * windows with title bars, redrawn full-screen each cycle (simplest
 * way to avoid cursor/drag artifacts without a dirty-rect tracker),
 * throttled to roughly 30Hz against the timer tick count.
 */

#include <kernel/types.h>
#include <arch/arch.h>
#include <gui/gui.h>

#define GUI_MAX_WINDOWS 4
#define TITLEBAR_HEIGHT 20
#define CURSOR_SIZE     10

typedef struct {
    i32 x, y;
    u32 w, h;
    u32 color;
    const char* title;
} window_t;

static window_t windows[GUI_MAX_WINDOWS];
static int window_count = 0;

static i32 cursor_x = 0, cursor_y = 0;
static u8  cursor_buttons = 0;
static bool prev_left_down = false;

static i32 drag_window = -1;
static i32 drag_offset_x = 0, drag_offset_y = 0;

static u64 last_redraw_tick = 0;

static void raise_window(int index)
{
    window_t tmp = windows[index];
    for (int j = index; j < window_count - 1; j++)
        windows[j] = windows[j + 1];
    windows[window_count - 1] = tmp;
}

static void handle_click(void)
{
    for (int i = window_count - 1; i >= 0; i--) {
        window_t* w = &windows[i];
        bool in_win = cursor_x >= w->x && cursor_x < w->x + (i32)w->w &&
                      cursor_y >= w->y && cursor_y < w->y + (i32)w->h;
        if (!in_win)
            continue;

        if (cursor_y < w->y + TITLEBAR_HEIGHT) {
            drag_offset_x = cursor_x - w->x;
            drag_offset_y = cursor_y - w->y;
        } else {
            w->color ^= 0x00202020;   /* visible click feedback */
        }

        raise_window(i);
        drag_window = window_count - 1;
        return;
    }
}

static void draw_window(const window_t* win)
{
    arch_gfx_fill_rect((u32)win->x, (u32)win->y, win->w, win->h, win->color);
    arch_gfx_fill_rect((u32)win->x, (u32)win->y, win->w, TITLEBAR_HEIGHT, 0x00222222);
    arch_gfx_draw_string((u32)win->x + 4, (u32)win->y + 6, win->title, 0x00FFFFFF, 0x00222222);
}

static void draw_cursor(void)
{
    arch_gfx_fill_rect((u32)cursor_x, (u32)cursor_y, CURSOR_SIZE, CURSOR_SIZE, 0x00FFFFFF);
    arch_gfx_fill_rect((u32)cursor_x + 1, (u32)cursor_y + 1, CURSOR_SIZE - 2, CURSOR_SIZE - 2, 0x00000000);
}

void gui_init(void)
{
    if (!arch_gfx_available())
        return;

    cursor_x = (i32)(arch_gfx_width()  / 2);
    cursor_y = (i32)(arch_gfx_height() / 2);

    windows[0] = (window_t){ .x = 40,  .y = 40,  .w = 220, .h = 140, .color = 0x00335577, .title = "MEGKER" };
    windows[1] = (window_t){ .x = 300, .y = 100, .w = 200, .h = 120, .color = 0x00553333, .title = "WINDOW 2" };
    window_count = 2;
}

void gui_update(void)
{
    if (!arch_gfx_available())
        return;

    i32 mx, my;
    u8  mbtn;
    if (arch_mouse_get_state(&mx, &my, &mbtn)) {
        cursor_x = mx;
        cursor_y = my;
        cursor_buttons = mbtn;
    }

    bool left_down = (cursor_buttons & 0x01) != 0;
    if (left_down && !prev_left_down)
        handle_click();
    if (!left_down)
        drag_window = -1;
    prev_left_down = left_down;

    if (drag_window >= 0) {
        windows[drag_window].x = cursor_x - drag_offset_x;
        windows[drag_window].y = cursor_y - drag_offset_y;
    }

    u64 now = arch_timer_ticks();
    if (now - last_redraw_tick < 3)   /* ~30Hz against a 100Hz timer */
        return;
    last_redraw_tick = now;

    arch_gfx_fill_rect(0, 0, arch_gfx_width(), arch_gfx_height(), 0x00102030);
    for (int i = 0; i < window_count; i++)
        draw_window(&windows[i]);
    draw_cursor();
}
