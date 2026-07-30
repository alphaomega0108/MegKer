/* arch/aarch64/arch.c
 * Implements the arch abstraction layer for aarch64, targeting
 * QEMU's `virt` machine. kernel_main() calls these — never touches
 * hardware directly.
 *
 * This is a first bring-up: console + memory only. No GIC, no timer
 * interrupt, no graphics, no disk, no scheduler context switch yet —
 * every such stub is called out below rather than hidden.
 */

#include <arch/arch.h>
#include <arch/aarch64/uart.h>
#include <kernel/kernel.h>
#include <kernel/types.h>
#include <mm/pmm.h>

/* QEMU's `virt` machine puts RAM at 0x40000000. Real device-tree
 * parsing to discover the actual installed size is future work (see
 * README) — for now this has to match the `-m` value in the Makefile. */
#define AARCH64_RAM_BASE 0x40000000ULL
#define AARCH64_RAM_SIZE (1024ULL * 1024 * 1024)   /* 1GB, matches -m 1G */

extern u8 _kernel_start[];
extern u8 _kernel_end[];
extern u8 vector_table[];   /* arch/aarch64/boot/exceptions.S */

const char* arch_name(void)
{
    return "aarch64";
}

void arch_early_init(void)
{
    uart_init();
}

void arch_late_init(void)
{
    /* GIC, PL011 RX interrupt, generic timer interrupt, PCI/virtio
     * disk: all future work — see README known limitations. */
}

void arch_console_init(void)   { }
void arch_console_putc(char c) { uart_putc(c); }
void arch_console_clear(void)  { }

char arch_keyboard_getchar(void)
{
    return 0;   /* no input device wired up yet */
}

bool arch_mouse_get_state(i32* x, i32* y, u8* buttons)
{
    UNUSED(x); UNUSED(y); UNUSED(buttons);
    return false;
}

bool arch_gfx_available(void) { return false; }
u32  arch_gfx_width(void)     { return 0; }
u32  arch_gfx_height(void)    { return 0; }
void arch_gfx_put_pixel(u32 x, u32 y, u32 rgb)                      { UNUSED(x); UNUSED(y); UNUSED(rgb); }
void arch_gfx_fill_rect(u32 x, u32 y, u32 w, u32 h, u32 rgb)        { UNUSED(x); UNUSED(y); UNUSED(w); UNUSED(h); UNUSED(rgb); }
void arch_gfx_draw_line(i32 x0, i32 y0, i32 x1, i32 y1, u32 rgb)    { UNUSED(x0); UNUSED(y0); UNUSED(x1); UNUSED(y1); UNUSED(rgb); }
void arch_gfx_draw_string(u32 x, u32 y, const char* s, u32 fg, u32 bg) { UNUSED(x); UNUSED(y); UNUSED(s); UNUSED(fg); UNUSED(bg); }

void arch_interrupts_init(void)
{
    /* No GIC set up yet, so nothing actually routes to the CPU — but
     * kernel_main() unmasks DAIF unconditionally right after this
     * call, so VBAR_EL1 must point at real handlers first rather than
     * its EL1 reset value of 0. */
    __asm__ volatile ("msr vbar_el1, %0" :: "r" (vector_table));
}

void arch_interrupts_enable(void)
{
    __asm__ volatile ("msr daifclr, #0xf");
}

void arch_interrupts_disable(void)
{
    __asm__ volatile ("msr daifset, #0xf");
}

void arch_mm_init(u64 boot_magic, void* boot_info)
{
    UNUSED(boot_magic);
    UNUSED(boot_info);   /* DTB pointer — not parsed yet, see README */

    mem_region_t region = {
        .base   = AARCH64_RAM_BASE,
        .length = AARCH64_RAM_SIZE,
    };
    pmm_init(&region, 1, (physaddr)_kernel_start, (physaddr)_kernel_end);
}

u64 arch_get_total_ram(void)
{
    return AARCH64_RAM_SIZE;
}

void arch_cpu_halt(void)
{
    while (1)
        __asm__ volatile ("wfe");
}

void arch_cpu_relax(void)
{
    __asm__ volatile ("yield");
}

void arch_timer_init(u32 hz)
{
    UNUSED(hz);   /* generic timer + GIC wiring: future work */
}

u64 arch_timer_ticks(void)
{
    return 0;   /* no timer interrupt driving this yet */
}

void* arch_thread_init_stack(void* stack_top, void (*entry)(void*), void* arg)
{
    UNUSED(stack_top); UNUSED(entry); UNUSED(arg);
    return NULL;   /* no context-switch mechanics on this arch yet */
}

void arch_context_switch(void** old_sp, void* new_sp)
{
    UNUSED(old_sp); UNUSED(new_sp);
}

bool arch_disk_available(void) { return false; }
bool arch_disk_read_sector(u32 lba, u8* buf) { UNUSED(lba); UNUSED(buf); return false; }
