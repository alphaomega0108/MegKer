/* arch/aarch64/boot/usermode.S
 * EL0 entry. Two pieces:
 *
 *   enter_usermode(entry, user_stack)
 *     Sets up SP_EL0/ELR_EL1/SPSR_EL1 by hand and erets to EL0.
 *     Never returns — the only way back to EL1 is a fresh exception
 *     (the SVC vector, in practice), which lands on SP_EL1 via
 *     kernel_entry, not here.
 *
 *   process_trampoline
 *     Reached via `ret` from arch_context_switch() (see
 *     boot/context_switch.S), exactly like a new kernel thread's
 *     trampoline — except this one's job is just to unpack a
 *     launch_params_t* (stashed in x19 by process.c's fake initial
 *     stack) into arguments and call enter_usermode with it.
 */

.section ".text"
.global enter_usermode
enter_usermode:
    /* x0 = entry, x1 = user_stack */
    msr sp_el0, x1
    msr elr_el1, x0
    mov x2, #0            /* SPSR: EL0t, DAIF all clear — EL0 must run with interrupts enabled */
    msr spsr_el1, x2
    eret

.global process_trampoline
process_trampoline:
    /* x19 = launch_params_t* { u64 entry; u64 user_stack; } */
    ldr x0, [x19, #0]
    ldr x1, [x19, #8]
    bl enter_usermode
    /* enter_usermode never returns */
hang:
    wfe
    b hang
