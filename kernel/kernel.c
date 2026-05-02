/* kernel/kernel.c
 * MegKer main kernel entry point.
 * This code is completely arch-independent.
 * It calls arch_* functions for anything hardware related.
 */

#include <kernel/kernel.h>
#include <kernel/types.h>
#include <arch/arch.h>

/* Global kernel state — readable from anywhere */
kernel_state_t kernel_state = KERNEL_STATE_BOOT;

void kernel_main(void)
{
    /* 1. Very first arch init — sets up console so we can print */
    arch_early_init();
    arch_console_init();

    /* 2. Print the banner */
    arch_console_putc('\n');
    arch_console_putc('M');
    arch_console_putc('e');
    arch_console_putc('g');
    arch_console_putc('K');
    arch_console_putc('e');
    arch_console_putc('r');
    arch_console_putc('\n');

    /* 3. Init memory */
    arch_mm_init();

    /* 4. Init interrupts */
    arch_interrupts_init();
    arch_interrupts_enable();

    /* 5. Init timer at 100Hz */
    arch_timer_init(100);

    /* 6. Late arch init */
    arch_late_init();

    /* 7. Kernel is fully up */
    kernel_state = KERNEL_STATE_RUNNING;

    /* 8. Main idle loop — never returns */
    while (1) {
        arch_cpu_relax();
    }
}

void kernel_panic(const char* msg)
{
    /* Disable interrupts immediately */
    arch_interrupts_disable();
    kernel_state = KERNEL_STATE_PANIC;

    /* Print PANIC message character by character */
    arch_console_putc('P');
    arch_console_putc('A');
    arch_console_putc('N');
    arch_console_putc('I');
    arch_console_putc('C');
    arch_console_putc(':');
    arch_console_putc(' ');

    /* Print the message */
    const char* p = msg;
    while (*p) {
        arch_console_putc(*p);
        p++;
    }

    /* Halt forever */
    arch_cpu_halt();
}