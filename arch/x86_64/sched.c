/* arch/x86_64/sched.c
 * Builds a new kernel thread's initial stack so the shared
 * arch_context_switch() in context_switch.asm can resume it like any
 * other suspended thread — see that file for the exact register
 * layout this has to match.
 */

#include <arch/arch.h>
#include <kernel/types.h>

extern void thread_trampoline(void);

void* arch_thread_init_stack(void* stack_top, void (*entry)(void*), void* arg)
{
    u64* sp = (u64*)stack_top;

    *(--sp) = (u64)thread_trampoline;  /* landed on via `ret` in arch_context_switch */
    *(--sp) = 0;                        /* rbp */
    *(--sp) = 0;                        /* rbx */
    *(--sp) = (u64)entry;               /* r12 — read by thread_trampoline */
    *(--sp) = (u64)arg;                 /* r13 — read by thread_trampoline */
    *(--sp) = 0;                        /* r14 */
    *(--sp) = 0;                        /* r15 */

    return sp;
}
