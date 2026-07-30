/* include/arch/aarch64/framebuffer.h
 * Linear graphics framebuffer, negotiated via QEMU's `ramfb` device
 * over the fw_cfg DMA interface (see drivers/framebuffer.c) — no
 * Multiboot2-style negotiation on this arch, so unlike x86_64 the
 * resolution is just a fixed constant this driver picks itself.
 */

#ifndef ARCH_AARCH64_FRAMEBUFFER_H
#define ARCH_AARCH64_FRAMEBUFFER_H

#include <kernel/types.h>

void fb_init(void);
bool fb_available(void);
u32  fb_width(void);
u32  fb_height(void);

void fb_put_pixel(u32 x, u32 y, u32 rgb);
void fb_fill_rect(u32 x, u32 y, u32 w, u32 h, u32 rgb);
void fb_draw_line(i32 x0, i32 y0, i32 x1, i32 y1, u32 rgb);
void fb_clear(u32 rgb);

/* 8x8 bitmap font — same glyph set as x86_64's; drivers/font8x8.c
 * here is an unmodified copy, since the font data itself has no
 * hardware dependency. */
void fb_draw_char(u32 x, u32 y, char c, u32 fg, u32 bg);
void fb_draw_string(u32 x, u32 y, const char* s, u32 fg, u32 bg);

#endif /* ARCH_AARCH64_FRAMEBUFFER_H */
