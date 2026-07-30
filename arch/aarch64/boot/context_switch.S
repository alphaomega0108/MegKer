/* arch/aarch64/boot/context_switch.S
 * arch_context_switch(void** old_sp, void* new_sp): saves the
 * callee-saved registers (AAPCS64: x19-x28, x29/FP, x30/LR) onto the
 * current stack, stashes the resulting SP into *old_sp, then loads
 * SP = new_sp and pops that thread's own previously-saved registers
 * — which were either pushed here by this same function on its last
 * suspend, or synthesized by arch_thread_init_stack() for a thread
 * that's never run yet. `ret` then resumes at whatever's in x30:
 * either back into the C call chain that suspended this thread, or
 * thread_trampoline for a brand-new one.
 */

.section ".text"
.global arch_context_switch
arch_context_switch:
    sub sp, sp, #96
    stp x19, x20, [sp, #0]
    stp x21, x22, [sp, #16]
    stp x23, x24, [sp, #32]
    stp x25, x26, [sp, #48]
    stp x27, x28, [sp, #64]
    stp x29, x30, [sp, #80]

    mov x2, sp
    str x2, [x0]                /* *old_sp = sp */

    mov sp, x1                  /* sp = new_sp */

    ldp x19, x20, [sp, #0]
    ldp x21, x22, [sp, #16]
    ldp x23, x24, [sp, #32]
    ldp x25, x26, [sp, #48]
    ldp x27, x28, [sp, #64]
    ldp x29, x30, [sp, #80]
    add sp, sp, #96

    ret

/* Entry point for a brand-new thread, reached via the `ret` above
 * when arch_thread_init_stack() built this thread's initial frame.
 * x19/x20 carry entry/arg through that save/restore cycle since
 * they're ordinary callee-saved registers as far as the switch code
 * is concerned. */
.global thread_trampoline
thread_trampoline:
    msr daifclr, #0xf           /* new threads must not inherit IF=masked
                                  * from the timer-IRQ context that spawned them */
    mov x0, x20                 /* arg */
    blr x19                     /* entry(arg) */
    bl thread_exit
