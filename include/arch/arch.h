/* include/arch/arch.h
 * Architecture abstraction layer.
 * The core kernel ONLY calls these functions — never arch code directly.
 * Each arch/ folder implements ALL of these.
 */

#ifndef ARCH_H
#define ARCH_H

#include <kernel/types.h>

/* --- Identity --- */
const char* arch_name(void);        /* returns "x86_64", "aarch64", etc */

/* --- Early boot --- */
void arch_early_init(void);         /* very first thing called         */
void arch_late_init(void);          /* called after kernel is ready    */

/* --- Console output --- */
void arch_console_init(void);       /* set up the console              */
void arch_console_putc(char c);     /* print one character             */
void arch_console_clear(void);      /* clear the screen                */

/* --- Keyboard input ---
 * Non-blocking: returns 0 if nothing is waiting. Archs without a
 * keyboard (yet) can just always return 0. */
char arch_keyboard_getchar(void);

/* --- Mouse input ---
 * Non-blocking. Returns false (and leaves outputs untouched) if
 * there's nothing new — archs without a pointing device can always
 * return false. buttons: bit0=left, bit1=right, bit2=middle. */
bool arch_mouse_get_state(i32* x, i32* y, u8* buttons);

/* --- Graphics ---
 * A linear RGB framebuffer, if the arch/platform has one. Archs
 * without graphics yet can leave arch_gfx_available() returning
 * false — everything else becomes a no-op in that case. Colors are
 * 0x00RRGGBB. */
bool arch_gfx_available(void);
u32  arch_gfx_width(void);
u32  arch_gfx_height(void);
void arch_gfx_put_pixel(u32 x, u32 y, u32 rgb);
void arch_gfx_fill_rect(u32 x, u32 y, u32 w, u32 h, u32 rgb);
void arch_gfx_draw_line(i32 x0, i32 y0, i32 x1, i32 y1, u32 rgb);
void arch_gfx_draw_string(u32 x, u32 y, const char* s, u32 fg, u32 bg);

/* --- Interrupts --- */
void arch_interrupts_init(void);    /* set up IDT/GIC/etc              */
void arch_interrupts_enable(void);  /* enable interrupts globally      */
void arch_interrupts_disable(void); /* disable interrupts globally     */

/* --- Memory ---
 * boot_magic/boot_info are whatever the bootloader handed kernel_main —
 * opaque outside arch code (on x86_64 that's the Multiboot2 magic and
 * info pointer; another arch might get a device tree blob instead). */
void arch_mm_init(u64 boot_magic, void* boot_info);
u64  arch_get_total_ram(void);      /* how much RAM do we have?        */

/* --- CPU control --- */
void arch_cpu_halt(void);           /* stop the CPU forever            */
void arch_cpu_relax(void);          /* pause hint inside loops         */

/* --- Timer --- */
void arch_timer_init(u32 hz);       /* set up timer at given frequency */
u64  arch_timer_ticks(void);        /* ticks since boot                */

#endif /* ARCH_H */