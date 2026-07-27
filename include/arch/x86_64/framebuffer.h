/* include/arch/x86_64/framebuffer.h
 * Linear graphics framebuffer, negotiated via the Multiboot2
 * framebuffer request tag (see boot.asm) and reported back in the
 * info tag GRUB hands us. Only direct RGB, 32bpp is supported —
 * that's what GRUB/QEMU's VBE negotiation gives by default; other
 * modes are left unmapped (fb_available() stays false).
 */

#ifndef ARCH_X86_64_FRAMEBUFFER_H
#define ARCH_X86_64_FRAMEBUFFER_H

#include <kernel/types.h>

typedef struct {
    bool     present;
    physaddr addr;
    u32      pitch;    /* bytes per scanline */
    u32      width;
    u32      height;
    u8       bpp;
} fb_info_t;

/* Provided by arch/x86_64/mm/multiboot2.c — parsed from the same tag
 * walk as the memory map. */
const fb_info_t* x86_64_get_framebuffer_info(void);

void fb_init(void);
bool fb_available(void);
u32  fb_width(void);
u32  fb_height(void);

void fb_put_pixel(u32 x, u32 y, u32 rgb);
void fb_fill_rect(u32 x, u32 y, u32 w, u32 h, u32 rgb);
void fb_draw_line(i32 x0, i32 y0, i32 x1, i32 y1, u32 rgb);
void fb_clear(u32 rgb);

/* 8x8 bitmap font. Only space/digits/uppercase/basic punctuation are
 * defined; lowercase is folded to uppercase, anything else renders
 * blank. */
void fb_draw_char(u32 x, u32 y, char c, u32 fg, u32 bg);
void fb_draw_string(u32 x, u32 y, const char* s, u32 fg, u32 bg);

#endif /* ARCH_X86_64_FRAMEBUFFER_H */
