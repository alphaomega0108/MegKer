/* include/arch/x86_64/pit.h
 * Programmable Interval Timer (8253/8254) — drives IRQ0.
 */

#ifndef ARCH_X86_64_PIT_H
#define ARCH_X86_64_PIT_H

#include <kernel/types.h>

void pit_init(u32 hz);
u64  pit_get_ticks(void);

#endif /* ARCH_X86_64_PIT_H */
