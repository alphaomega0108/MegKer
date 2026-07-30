/* arch/aarch64/arch.c
 * Implements the arch abstraction layer for aarch64, targeting
 * QEMU's `virt` machine. kernel_main() calls these — never touches
 * hardware directly.
 *
 * Boot/console/memory/interrupts/timer/scheduler are real. No
 * graphics, no input, no disk yet — every such stub is called out
 * below rather than hidden.
 */

#include <arch/arch.h>
#include <arch/aarch64/gic.h>
#include <arch/aarch64/mmu.h>
#include <arch/aarch64/process.h>
#include <arch/aarch64/timer.h>
#include <arch/aarch64/uart.h>
#include <arch/aarch64/virtio_blk.h>
#include <arch/aarch64/vmm.h>
#include <kernel/kernel.h>
#include <kernel/types.h>
#include <mm/pmm.h>

/* Embedded by the Makefile (ld -r -b binary) from the built
 * userland/hello_aarch64.c — a real static ELF64 test executable. */
extern const u8 _binary_hello_elf_start[];
extern const u8 _binary_hello_elf_end[];

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
    /* Turns the MMU on — must run before anything else so every
     * subsequent access (console, memory, interrupts) is a virtual
     * one from the very start, the same as x86_64's paging being
     * live before its first C instruction. */
    aarch64_mmu_init();
    uart_init();
}

static void puts(const char* s)
{
    while (*s)
        uart_putc(*s++);
}

/* Proves address-space isolation actually works, mirroring x86_64's
 * vmm_selftest(): map a fresh frame at a virtual address only inside
 * a brand-new address space, confirm it reads back correctly there,
 * and confirm it's genuinely invisible from the kernel's own space. */
static bool vmm_selftest(void)
{
    const virtaddr test_va = 0x80000000ULL;   /* 2GB — past both boot L1 blocks (device + RAM), untouched */

    if (vmm_translate(vmm_kernel_space(), test_va) != 0)
        return false;   /* not a clean test address */

    address_space_t* as = vmm_create_address_space();
    physaddr frame = pmm_alloc_frame();
    if (!frame)
        return false;

    vmm_map(as, test_va, frame, VMM_PRESENT | VMM_WRITABLE);

    vmm_switch(as);
    *(volatile u32*)test_va = 0xDEADBEEF;
    bool readback_ok = (*(volatile u32*)test_va == 0xDEADBEEF);
    vmm_switch(vmm_kernel_space());

    bool isolated_ok = (vmm_translate(vmm_kernel_space(), test_va) == 0);
    bool mapped_ok    = (vmm_translate(as, test_va) == (frame | 0));

    vmm_destroy_address_space(as);
    pmm_free_frame(frame);

    return readback_ok && isolated_ok && mapped_ok;
}

static void put_dec(u32 v)
{
    char buf[11];
    int i = 10;
    buf[i] = '\0';
    if (v == 0) {
        uart_putc('0');
        return;
    }
    while (v > 0) {
        buf[--i] = (char)('0' + (v % 10));
        v /= 10;
    }
    puts(&buf[i]);
}

void arch_late_init(void)
{
    bool ok = vmm_selftest();
    puts(ok ? "VMM: OK\n" : "VMM: FAIL\n");
    if (!ok)
        kernel_panic("VMM self-test failed");

    usize hello_size = (usize)(_binary_hello_elf_end - _binary_hello_elf_start);
    i32 exit_code = process_run(_binary_hello_elf_start, hello_size);
    if (exit_code < 0) {
        puts("USERLAND: LOAD FAILED\n");
    } else {
        puts("USERLAND: EXIT ");
        put_dec((u32)exit_code);
        puts("\n");
    }

    virtio_blk_init();

    /* PL011 RX interrupt, graphics: future work — see README known
     * limitations. */
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
    __asm__ volatile ("msr vbar_el1, %0" :: "r" (vector_table));
    gic_init();
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
    vmm_init();
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
    generic_timer_init(hz);
}

u64 arch_timer_ticks(void)
{
    return generic_timer_ticks();
}

/* arch_thread_init_stack() and arch_context_switch() are implemented
 * in sched.c / boot/context_switch.S — real context-switch mechanics
 * now that the timer IRQ drives sched_tick(). */

bool arch_disk_available(void) { return virtio_blk_available(); }
bool arch_disk_read_sector(u32 lba, u8* buf) { return virtio_blk_read_sector(lba, buf); }
