/* include/arch/aarch64/timer.h
 * ARM Generic Timer, non-secure EL1 physical timer (CNTP_*), routed
 * through the GIC as PPI 30 — the standard IRQ for this timer on
 * QEMU's `virt` machine (and real hardware following the same
 * arm,armv8-timer device-tree binding).
 */

#ifndef ARCH_AARCH64_TIMER_H
#define ARCH_AARCH64_TIMER_H

#include <kernel/types.h>

#define GENERIC_TIMER_IRQ 30

void generic_timer_init(u32 hz);
u64  generic_timer_ticks(void);

/* Split in two on purpose — called by the IRQ dispatcher
 * (arch/aarch64/irq.c) around its GIC EOI, not as one step:
 *
 * generic_timer_ack() reloads CNTP_TVAL_EL0, clearing the timer's own
 * asserted condition. It must run BEFORE EOI — this is a
 * level-triggered PPI, so EOI alone doesn't clear it, and the GIC
 * re-presents a still-asserted line almost immediately.
 *
 * generic_timer_tick() does the actual tick bookkeeping and calls
 * sched_tick(), which may trigger a context switch that never
 * returns here — same reason x86_64's PIC EOI has to happen before
 * its handler runs at all. It must run AFTER EOI. */
void generic_timer_ack(void);
void generic_timer_tick(void);

#endif /* ARCH_AARCH64_TIMER_H */
