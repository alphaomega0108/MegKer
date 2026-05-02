/* include/arch/arch.h
 * Architecture abstraction layer.
 * The core kernel ONLY calls these functions — never arch code directly.
 * Each arch/ folder implements ALL of these.
 */

#ifndef ARCH_H
#define ARCH_H

#include <kernel/types.h>

/* --- Identity --- */
const char* arch_name(void);        /* returns "x86_64", "aarch64", etc */

/* --- Early boot --- */
void arch_early_init(void);         /* very first thing called         */
void arch_late_init(void);          /* called after kernel is ready    */

/* --- Console output --- */
void arch_console_init(void);       /* set up the console              */
void arch_console_putc(char c);     /* print one character             */
void arch_console_clear(void);      /* clear the screen                */

/* --- Interrupts --- */
void arch_interrupts_init(void);    /* set up IDT/GIC/etc              */
void arch_interrupts_enable(void);  /* enable interrupts globally      */
void arch_interrupts_disable(void); /* disable interrupts globally     */

/* --- Memory --- */
void arch_mm_init(void);            /* set up paging/MMU               */
u64  arch_get_total_ram(void);      /* how much RAM do we have?        */

/* --- CPU control --- */
void arch_cpu_halt(void);           /* stop the CPU forever            */
void arch_cpu_relax(void);          /* pause hint inside loops         */

/* --- Timer --- */
void arch_timer_init(u32 hz);       /* set up timer at given frequency */
u64  arch_timer_ticks(void);        /* ticks since boot                */

#endif /* ARCH_H */