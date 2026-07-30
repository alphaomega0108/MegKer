/* include/arch/aarch64/gic.h
 * Minimal GICv2 driver — just enough to enable one PPI (the generic
 * timer) and acknowledge/EOI it. QEMU's `virt` machine maps the
 * Distributor and CPU interface at fixed addresses.
 */

#ifndef ARCH_AARCH64_GIC_H
#define ARCH_AARCH64_GIC_H

#include <kernel/types.h>

void gic_init(void);
void gic_enable_irq(u32 irq);

/* Acknowledge the highest-priority pending interrupt; low 10 bits of
 * the return value are the INTID (pass the whole value back to
 * gic_cpu_eoi() unchanged — GICv2 EOI wants the same source field
 * bits it came with, not just the INTID). */
u32  gic_cpu_iack(void);
void gic_cpu_eoi(u32 iar);

#endif /* ARCH_AARCH64_GIC_H */
