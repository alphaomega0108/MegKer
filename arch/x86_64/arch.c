/* arch/x86_64/arch.c
 * Implements the arch abstraction layer for x86_64.
 * kernel_main() calls these — never touches hardware directly.
 */

#include <arch/arch.h>
#include <arch/x86_64/framebuffer.h>
#include <arch/x86_64/idt.h>
#include <arch/x86_64/keyboard.h>
#include <arch/x86_64/mm.h>
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
    if (fb_available()) {
        fb_clear(0x001A2233);
        fb_fill_rect(20, 20, 200, 100, 0x00CC3333);
        fb_draw_line(20, 140, 220, 140, 0x0033CC33);
        fb_draw_string(20, 160, "MEGKER GRAPHICS OK", 0x00FFFFFF, 0x001A2233);
    }

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

void arch_timer_init(u32 hz)
{
    pit_init(hz);
}

u64 arch_timer_ticks(void)
{
    return pit_get_ticks();
}