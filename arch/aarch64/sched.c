/* arch/aarch64/sched.c
 * Builds a new kernel thread's initial stack so the shared
 * arch_context_switch() in context_switch.S can resume it like any
 * other suspended thread — see that file for the exact register
 * layout this has to match.
 */

#include <arch/arch.h>
#include <kernel/types.h>

extern void thread_trampoline(void);

void* arch_thread_init_stack(void* stack_top, void (*entry)(void*), void* arg)
{
    u64* sp = (u64*)stack_top;

    *(--sp) = (u64)thread_trampoline;  /* x30 — landed on via `ret` in arch_context_switch */
    *(--sp) = 0;                        /* x29 */
    *(--sp) = 0;                        /* x28 */
    *(--sp) = 0;                        /* x27 */
    *(--sp) = 0;                        /* x26 */
    *(--sp) = 0;                        /* x25 */
    *(--sp) = 0;                        /* x24 */
    *(--sp) = 0;                        /* x23 */
    *(--sp) = 0;                        /* x22 */
    *(--sp) = 0;                        /* x21 */
    *(--sp) = (u64)arg;                 /* x20 — read by thread_trampoline */
    *(--sp) = (u64)entry;               /* x19 — read by thread_trampoline */

    return sp;
}
