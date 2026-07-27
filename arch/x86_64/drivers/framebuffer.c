/* arch/x86_64/drivers/framebuffer.c
 * Linear graphics framebuffer — pixel/line/rect primitives and
 * 8x8 bitmap text on top of whatever GRUB negotiated for us.
 */

#include <arch/x86_64/framebuffer.h>
#include <arch/x86_64/paging.h>
#include <kernel/types.h>

extern const u8* font8x8_glyph(char c);

static const fb_info_t* fb = NULL;

void fb_init(void)
{
    fb = x86_64_get_framebuffer_info();
    if (!fb->present)
        return;

    /* The framebuffer's physical address is usually well above the
     * boot-time 1GB identity map (it's PCI MMIO) — make sure it's
     * actually mapped before anything touches it. */
    paging_identity_map(fb->addr, (u64)fb->pitch * fb->height);
}

bool fb_available(void) { return fb && fb->present; }
u32  fb_width(void)     { return fb ? fb->width  : 0; }
u32  fb_height(void)    { return fb ? fb->height : 0; }

void fb_put_pixel(u32 x, u32 y, u32 rgb)
{
    if (!fb || !fb->present || x >= fb->width || y >= fb->height)
        return;

    u8* row = (u8*)(usize)fb->addr + (usize)y * fb->pitch;
    *(u32*)(row + (usize)x * 4) = rgb & 0x00FFFFFFu;
}

void fb_fill_rect(u32 x, u32 y, u32 w, u32 h, u32 rgb)
{
    for (u32 row = y; row < y + h; row++)
        for (u32 col = x; col < x + w; col++)
            fb_put_pixel(col, row, rgb);
}

void fb_clear(u32 rgb)
{
    if (!fb || !fb->present)
        return;
    fb_fill_rect(0, 0, fb->width, fb->height, rgb);
}

void fb_draw_line(i32 x0, i32 y0, i32 x1, i32 y1, u32 rgb)
{
    /* Bresenham's line algorithm. */
    i32 dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    i32 sx = (x0 < x1) ? 1 : -1;
    i32 dy = (y1 > y0) ? (y0 - y1) : (y1 - y0);   /* negative magnitude */
    i32 sy = (y0 < y1) ? 1 : -1;
    i32 err = dx + dy;

    while (1) {
        fb_put_pixel((u32)x0, (u32)y0, rgb);
        if (x0 == x1 && y0 == y1)
            break;
        i32 e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void fb_draw_char(u32 x, u32 y, char c, u32 fg, u32 bg)
{
    const u8* g = font8x8_glyph(c);

    for (int row = 0; row < 8; row++) {
        u8 bits = g[row];
        for (int col = 0; col < 8; col++) {
            bool on = (bits >> (7 - col)) & 1;
            fb_put_pixel(x + (u32)col, y + (u32)row, on ? fg : bg);
        }
    }
}

void fb_draw_string(u32 x, u32 y, const char* s, u32 fg, u32 bg)
{
    u32 cx = x, cy = y;
    for (; *s; s++) {
        if (*s == '\n') {
            cx = x;
            cy += 8;
            continue;
        }
        fb_draw_char(cx, cy, *s, fg, bg);
        cx += 8;
    }
}
