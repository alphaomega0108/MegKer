/* arch/x86_64/drivers/console.c
 * VGA text mode console for x86_64.
 * VGA text buffer lives at physical address 0xB8000.
 * Each character = 2 bytes: [ascii][color]
 */

#include <kernel/types.h>

#define VGA_ADDRESS     0xB8000
#define VGA_COLS        80
#define VGA_ROWS        25
#define VGA_COLOR       0x0F    /* white text on black */

/* Current cursor position */
static u32 cursor_row = 0;
static u32 cursor_col = 0;

/* Pointer to VGA buffer */
static u16* vga = (u16*)VGA_ADDRESS;

/* Build a VGA character cell */
static inline u16 vga_char(char c, u8 color)
{
    return (u16)c | ((u16)color << 8);
}

void arch_console_clear(void)
{
    for (u32 r = 0; r < VGA_ROWS; r++)
        for (u32 c = 0; c < VGA_COLS; c++)
            vga[r * VGA_COLS + c] = vga_char(' ', VGA_COLOR);

    cursor_row = 0;
    cursor_col = 0;
}

static void scroll(void)
{
    /* Move every row up by one */
    for (u32 r = 1; r < VGA_ROWS; r++)
        for (u32 c = 0; c < VGA_COLS; c++)
            vga[(r-1) * VGA_COLS + c] = vga[r * VGA_COLS + c];

    /* Clear the last row */
    for (u32 c = 0; c < VGA_COLS; c++)
        vga[(VGA_ROWS-1) * VGA_COLS + c] = vga_char(' ', VGA_COLOR);

    cursor_row = VGA_ROWS - 1;
}

void arch_console_putc(char c)
{
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else if (c == '\r') {
        cursor_col = 0;
    } else if (c == '\t') {
        cursor_col = ALIGN_UP(cursor_col + 1, 4);
    } else {
        vga[cursor_row * VGA_COLS + cursor_col] = vga_char(c, VGA_COLOR);
        cursor_col++;
    }

    /* Wrap to next line */
    if (cursor_col >= VGA_COLS) {
        cursor_col = 0;
        cursor_row++;
    }

    /* Scroll if we hit the bottom */
    if (cursor_row >= VGA_ROWS)
        scroll();
}

void arch_console_init(void)
{
    arch_console_clear();
}