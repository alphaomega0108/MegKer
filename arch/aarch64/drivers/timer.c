/* arch/aarch64/drivers/timer.c
 * ARM Generic Timer driver — non-secure EL1 physical timer, periodic
 * via reload-on-fire (CNTP_TVAL_EL0 counts down to 0, we reload it
 * each time it fires rather than using an absolute CNTP_CVAL_EL0
 * comparator).
 */

#include <arch/aarch64/timer.h>
#include <arch/aarch64/gic.h>
#include <kernel/sched.h>

#define CNTP_CTL_ENABLE (1u << 0)

static u32 reload_ticks;
static volatile u64 ticks = 0;

static inline u64 read_cntfrq(void)
{
    u64 v;
    __asm__ volatile ("mrs %0, cntfrq_el0" : "=r" (v));
    return v;
}

static inline void write_cntp_tval(u64 v)
{
    __asm__ volatile ("msr cntp_tval_el0, %0" :: "r" (v));
}

static inline void write_cntp_ctl(u64 v)
{
    __asm__ volatile ("msr cntp_ctl_el0, %0" :: "r" (v));
}

void generic_timer_init(u32 hz)
{
    reload_ticks = (u32)(read_cntfrq() / hz);

    write_cntp_tval(reload_ticks);
    write_cntp_ctl(CNTP_CTL_ENABLE);   /* IMASK=0: enabled, unmasked */

    gic_enable_irq(GENERIC_TIMER_IRQ);
}

void generic_timer_ack(void)
{
    write_cntp_tval(reload_ticks);     /* reload for the next period */
}

void generic_timer_tick(void)
{
    ticks++;
    sched_tick();
}

u64 generic_timer_ticks(void)
{
    return ticks;
}
