/* arch/aarch64/drivers/framebuffer.c
 * Linear graphics framebuffer via QEMU's `ramfb` device — a simple
 * "give me a plain buffer" display, chosen over full virtio-gpu to
 * keep the same "pixel/line/rect/text on a raw buffer" model
 * x86_64's framebuffer driver already has, instead of a 2D command
 * queue. ramfb is configured over the fw_cfg DMA interface, which
 * this file also implements — used for nothing else right now, so
 * it isn't split into its own module.
 *
 * fw_cfg's wire format is big-endian regardless of guest endianness;
 * this kernel runs little-endian AArch64, so every multi-byte field
 * gets byte-swapped explicitly.
 */

#include <arch/aarch64/framebuffer.h>
#include <mm/pmm.h>

extern const u8* font8x8_glyph(char c);

#define FW_CFG_BASE     0x09020000UL
#define FW_CFG_DATA     (FW_CFG_BASE + 0x00)
#define FW_CFG_SELECTOR (FW_CFG_BASE + 0x08)
#define FW_CFG_DMA_ADDR (FW_CFG_BASE + 0x10)

#define FW_CFG_FILE_DIR 0x19u

#define FW_CFG_DMA_CTL_ERROR  (1u << 0)
#define FW_CFG_DMA_CTL_SELECT (1u << 3)
#define FW_CFG_DMA_CTL_WRITE  (1u << 4)

typedef struct {
    u32 control;
    u32 length;
    u64 address;
} __attribute__((packed)) fw_cfg_dma_access_t;   /* all fields big-endian on the wire */

#define RAMFB_FORMAT_XRGB8888 0x34325258u   /* DRM fourcc "XR24" */

typedef struct {
    u64 addr;
    u32 fourcc;
    u32 flags;
    u32 width;
    u32 height;
    u32 stride;
} __attribute__((packed)) ramfb_cfg_t;   /* all fields big-endian on the wire */

#define RAMFB_WIDTH  800
#define RAMFB_HEIGHT 600
#define RAMFB_BPP    4

typedef struct {
    bool     present;
    physaddr addr;
    u32      pitch;
    u32      width;
    u32      height;
} fb_state_t;

static fb_state_t fb = { 0 };

static inline u16 bswap16(u16 v) { return (u16)((v << 8) | (v >> 8)); }
static inline u32 bswap32(u32 v)
{
    return ((v & 0xFFu) << 24) | ((v & 0xFF00u) << 8) |
           ((v >> 8) & 0xFF00u) | ((v >> 24) & 0xFFu);
}
static inline u64 bswap64(u64 v)
{
    return ((u64)bswap32((u32)v) << 32) | bswap32((u32)(v >> 32));
}

static bool str_eq(const char* a, const char* b)
{
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static void fw_cfg_select(u16 key)
{
    *(volatile u16*)FW_CFG_SELECTOR = bswap16(key);
}

static u8 fw_cfg_read_byte(void)
{
    return *(volatile u8*)FW_CFG_DATA;
}

/* Walks the fw_cfg file directory (a fixed, well-known selector) to
 * find a file's own selector by name — the only way to address
 * dynamically-registered files like "etc/ramfb". */
static u16 fw_cfg_find_file(const char* name)
{
    fw_cfg_select(FW_CFG_FILE_DIR);

    u32 count = 0;
    for (int i = 0; i < 4; i++)
        count = (count << 8) | fw_cfg_read_byte();

    for (u32 i = 0; i < count; i++) {
        u8 entry[64];   /* {u32 size; u16 select; u16 reserved; char name[56];} */
        for (int b = 0; b < 64; b++)
            entry[b] = fw_cfg_read_byte();

        u16 select = (u16)((entry[4] << 8) | entry[5]);
        if (str_eq((const char*)&entry[8], name))
            return select;
    }
    return 0;
}

static bool fw_cfg_dma_write(u16 selector, const void* data, u32 length)
{
    static volatile fw_cfg_dma_access_t access __attribute__((aligned(8)));

    access.control = bswap32(((u32)selector << 16) | FW_CFG_DMA_CTL_SELECT | FW_CFG_DMA_CTL_WRITE);
    access.length  = bswap32(length);
    access.address = bswap64((u64)(usize)data);

    __asm__ volatile ("dmb ish" ::: "memory");
    *(volatile u64*)FW_CFG_DMA_ADDR = bswap64((u64)(usize)&access);

    u32 ctl;
    do {
        __asm__ volatile ("dmb ish" ::: "memory");
        ctl = bswap32(access.control);
    } while (ctl != 0 && !(ctl & FW_CFG_DMA_CTL_ERROR));

    return ctl == 0;
}

void fb_init(void)
{
    u16 sel = fw_cfg_find_file("etc/ramfb");
    if (sel == 0)
        return;   /* no ramfb device attached */

    usize fb_bytes = (usize)RAMFB_WIDTH * RAMFB_HEIGHT * RAMFB_BPP;
    u64 frames = (fb_bytes + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE;
    physaddr buf = pmm_alloc_frames(frames);
    if (!buf)
        return;

    ramfb_cfg_t cfg = {
        .addr   = bswap64(buf),
        .fourcc = bswap32(RAMFB_FORMAT_XRGB8888),
        .flags  = 0,
        .width  = bswap32(RAMFB_WIDTH),
        .height = bswap32(RAMFB_HEIGHT),
        .stride = bswap32(RAMFB_WIDTH * RAMFB_BPP),
    };
    if (!fw_cfg_dma_write(sel, &cfg, sizeof(cfg)))
        return;

    fb.present = true;
    fb.addr    = buf;
    fb.pitch   = RAMFB_WIDTH * RAMFB_BPP;
    fb.width   = RAMFB_WIDTH;
    fb.height  = RAMFB_HEIGHT;
}

bool fb_available(void) { return fb.present; }
u32  fb_width(void)     { return fb.width; }
u32  fb_height(void)    { return fb.height; }

void fb_put_pixel(u32 x, u32 y, u32 rgb)
{
    if (!fb.present || x >= fb.width || y >= fb.height)
        return;

    u8* row = (u8*)(usize)fb.addr + (usize)y * fb.pitch;
    *(u32*)(row + (usize)x * 4) = rgb & 0x00FFFFFFu;
}

void fb_fill_rect(u32 x, u32 y, u32 w, u32 h, u32 rgb)
{
    if (!fb.present)
        return;

    u32 x1 = x + w, y1 = y + h;
    if (x1 > fb.width)  x1 = fb.width;
    if (y1 > fb.height) y1 = fb.height;
    if (x >= x1 || y >= y1)
        return;

    u32 color = rgb & 0x00FFFFFFu;
    for (u32 row = y; row < y1; row++) {
        u32* p = (u32*)((u8*)(usize)fb.addr + (usize)row * fb.pitch + (usize)x * 4);
        for (u32 col = x; col < x1; col++)
            *p++ = color;
    }
}

void fb_clear(u32 rgb)
{
    if (!fb.present)
        return;
    fb_fill_rect(0, 0, fb.width, fb.height, rgb);
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
