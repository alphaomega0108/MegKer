/* arch/x86_64/drivers/pit.c
 * Programmable Interval Timer — the classic 1.193182 MHz crystal
 * behind IRQ0. We run channel 0 in mode 3 (square wave) to get a
 * periodic tick at the requested frequency.
 */

#include <arch/x86_64/pit.h>
#include <arch/x86_64/idt.h>
#include <arch/x86_64/io.h>
#include <kernel/sched.h>

#define PIT_FREQUENCY 1193182u
#define PIT_CHANNEL0  0x40
#define PIT_COMMAND   0x43

static volatile u64 ticks = 0;

static void pit_tick(registers_t* regs)
{
    UNUSED(regs);
    ticks++;
    sched_tick();
}

void pit_init(u32 hz)
{
    u16 divisor = (u16)(PIT_FREQUENCY / hz);

    outb(PIT_COMMAND, 0x36);   /* channel 0, lo/hi byte, mode 3, binary */
    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);

    irq_install_handler(0, pit_tick);
}

u64 pit_get_ticks(void)
{
    return ticks;
}
