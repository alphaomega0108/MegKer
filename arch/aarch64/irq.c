/* arch/aarch64/irq.c
 * IRQ dispatch, called from the vector table's IRQ (Current EL, SPx)
 * entry in boot/exceptions.S after it saves the full register frame.
 */

#include <arch/aarch64/gic.h>
#include <arch/aarch64/timer.h>

void aarch64_irq_handler(void)
{
    u32 iar    = gic_cpu_iack();
    u32 intid  = iar & 0x3FFu;

    /* Order matters twice over here:
     *  1. Clear the timer's own asserted condition BEFORE EOI — it's
     *     a level-triggered PPI, so EOI alone doesn't clear it, and
     *     the GIC re-presents a still-asserted line almost instantly.
     *  2. EOI BEFORE the tick/sched_tick() logic — a context switch
     *     in there can abandon this exact call frame until this
     *     thread is resumed again later, and EOI must not wait on
     *     that (same class of bug the x86_64 PIC path hit before its
     *     EOI was moved earlier). */
    if (intid == GENERIC_TIMER_IRQ)
        generic_timer_ack();

    gic_cpu_eoi(iar);

    if (intid == GENERIC_TIMER_IRQ)
        generic_timer_tick();
}
