/* arch/x86_64/arch.c
 * Implements the arch abstraction layer for x86_64.
 * kernel_main() calls these — never touches hardware directly.
 */

#include <arch/arch.h>
#include <arch/x86_64/framebuffer.h>
#include <arch/x86_64/idt.h>
#include <arch/x86_64/keyboard.h>
#include <arch/x86_64/mm.h>
#include <arch/x86_64/mouse.h>
#include <arch/x86_64/pit.h>
#include <kernel/types.h>

/* Defined in their own files */
extern void gdt_init(void);
extern void arch_console_init(void);
extern void arch_console_putc(char c);
extern void arch_console_clear(void);

const char* arch_name(void)
{
    return "x86_64";
}

void arch_early_init(void)
{
    gdt_init();
}

void arch_late_init(void)
{
    keyboard_init();

    fb_init();
    if (fb_available())
        mouse_init();

    /* ACPI, SMP later */
}

void arch_mm_init(u64 boot_magic, void* boot_info)
{
    x86_64_mm_init(boot_magic, boot_info);
}

u64 arch_get_total_ram(void)
{
    return x86_64_total_ram();
}

void arch_interrupts_init(void)
{
    idt_init();
}

void arch_interrupts_enable(void)
{
    __asm__ volatile ("sti");
}

void arch_interrupts_disable(void)
{
    __asm__ volatile ("cli");
}

void arch_cpu_halt(void)
{
    __asm__ volatile ("cli; hlt");
    while (1); /* never returns */
}

void arch_cpu_relax(void)
{
    __asm__ volatile ("pause");
}

char arch_keyboard_getchar(void)
{
    return keyboard_getchar();
}

bool arch_mouse_get_state(i32* x, i32* y, u8* buttons)
{
    return mouse_get_state(x, y, buttons);
}

bool arch_gfx_available(void) { return fb_available(); }
u32  arch_gfx_width(void)     { return fb_width(); }
u32  arch_gfx_height(void)    { return fb_height(); }

void arch_gfx_put_pixel(u32 x, u32 y, u32 rgb)              { fb_put_pixel(x, y, rgb); }
void arch_gfx_fill_rect(u32 x, u32 y, u32 w, u32 h, u32 rgb) { fb_fill_rect(x, y, w, h, rgb); }
void arch_gfx_draw_line(i32 x0, i32 y0, i32 x1, i32 y1, u32 rgb) { fb_draw_line(x0, y0, x1, y1, rgb); }
void arch_gfx_draw_string(u32 x, u32 y, const char* s, u32 fg, u32 bg) { fb_draw_string(x, y, s, fg, bg); }

void arch_timer_init(u32 hz)
{
    pit_init(hz);
}

u64 arch_timer_ticks(void)
{
    return pit_get_ticks();
}