/* kernel/kernel.c
 * MegKer main kernel entry point.
 * This code is completely arch-independent.
 * It calls arch_* functions for anything hardware related.
 */

#include <kernel/kernel.h>
#include <kernel/types.h>
#include <arch/arch.h>
#include <mm/heap.h>
#include <gui/gui.h>
#include <kernel/sched.h>
#include <fs/vfs.h>

/* Global kernel state — readable from anywhere */
kernel_state_t kernel_state = KERNEL_STATE_BOOT;

static void console_puts(const char* s)
{
    while (*s)
        arch_console_putc(*s++);
}

static void console_put_dec(u64 value)
{
    char buf[21];   /* max u64 is 20 digits + NUL */
    int i = 20;

    buf[i] = '\0';
    if (value == 0) {
        arch_console_putc('0');
        return;
    }
    while (value > 0) {
        buf[--i] = (char)('0' + (value % 10));
        value /= 10;
    }
    console_puts(&buf[i]);
}

/* Proves preemptive scheduling actually works: toggles a small
 * indicator square roughly once a second while the GUI thread (see
 * below) keeps running independently. */
static void blinker_thread(void* arg)
{
    UNUSED(arg);
    bool on = false;
    u64 last_tick = 0;

    while (1) {
        u64 now = arch_timer_ticks();
        if (now - last_tick >= 100) {   /* ~1s at the 100Hz timer */
            last_tick = now;
            on = !on;
            if (arch_gfx_available())
                arch_gfx_fill_rect(arch_gfx_width() - 20, 10, 10, 10,
                                    on ? 0x0000FF00 : 0x00FF0000);
        }
        arch_cpu_relax();
    }
}

void kernel_main(u64 boot_magic, void* boot_info)
{
    /* 1. Very first arch init — sets up console so we can print */
    arch_early_init();
    arch_console_init();

    /* 2. Print the banner */
    console_puts("\nMegKer\n");

    /* 3. Init memory */
    arch_mm_init(boot_magic, boot_info);
    console_puts("RAM: ");
    console_put_dec(arch_get_total_ram() / (1024 * 1024));
    console_puts(" MB\n");

    heap_init();
    void* heap_check = kmalloc(64);
    console_puts("Heap: ");
    console_puts(heap_check ? "OK\n" : "FAIL\n");
    kfree(heap_check);

    /* 4. Init interrupts */
    arch_interrupts_init();
    arch_interrupts_enable();

    /* 5. Init timer at 100Hz */
    arch_timer_init(100);

    /* 6. Late arch init */
    arch_late_init();

    /* 6a. Filesystem — proves disk -> driver -> VFS end to end */
    vfs_init();
    if (vfs_available()) {
        char filebuf[128];
        u32 filesize = 0;
        bool ok = vfs_read_file("hello.txt", filebuf, sizeof(filebuf) - 1, &filesize);
        if (ok) {
            u32 shown = filesize < sizeof(filebuf) - 1 ? filesize : sizeof(filebuf) - 1;
            filebuf[shown] = '\0';
        }
        if (arch_gfx_available())
            arch_gfx_draw_string(10, 30, ok ? filebuf : "VFS: READ FAILED",
                                  ok ? 0x0000FFFF : 0x00FF0000, 0x00102030);
        else
            console_puts(ok ? filebuf : "VFS: READ FAILED\n");
    } else if (arch_gfx_available()) {
        arch_gfx_draw_string(10, 30, "VFS: NOT AVAILABLE", 0x00FF0000, 0x00102030);
    }

    gui_init();

    /* 6b. Scheduler — this call stack becomes thread 0 */
    sched_init();
    thread_create(blinker_thread, NULL);

    /* 7. Kernel is fully up */
    kernel_state = KERNEL_STATE_RUNNING;

    /* 8. Main idle loop — GUI when available, echo otherwise */
    while (1) {
        if (arch_gfx_available()) {
            gui_update();
        } else {
            char c = arch_keyboard_getchar();
            if (c)
                arch_console_putc(c);
        }
        arch_cpu_relax();
    }
}

void kernel_panic(const char* msg)
{
    /* Disable interrupts immediately */
    arch_interrupts_disable();
    kernel_state = KERNEL_STATE_PANIC;

    console_puts("PANIC: ");
    console_puts(msg);

    /* Halt forever */
    arch_cpu_halt();
}