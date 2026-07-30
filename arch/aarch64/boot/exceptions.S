/* arch/aarch64/boot/exceptions.S
 * AArch64 exception vector table. Real handlers exist for:
 *   - IRQ, current EL SPx (index 5)      — kernel-thread-sourced IRQ
 *   - SVC, lower EL AArch64 (index 8)    — EL0 syscalls
 *   - IRQ, lower EL AArch64 (index 9)    — timer preempting an EL0 process
 * EL1 always runs SPSel=1 (EL1h), so SP_EL1 is the active stack no
 * matter which of these fires — kernel_entry/kernel_exit need no
 * per-source-EL variant. Every other entry (FIQ, SError, EL0
 * synchronous faults other than SVC, AArch32) still halts rather than
 * silently running whatever garbage the fault landed on.
 */

.macro vector_stub
    b unhandled_exception
    .balign 0x80
.endm

/* Saves x0-x30 + ELR_EL1 + SPSR_EL1 — the full context, since the
 * interrupted code could have been using any of them and our C
 * handler is free to clobber caller-saved registers. 34 slots * 8
 * bytes = 272, a multiple of 16 (SP must stay 16-byte aligned). */
.macro kernel_entry
    sub sp, sp, #272
    stp x0,  x1,  [sp, #0]
    stp x2,  x3,  [sp, #16]
    stp x4,  x5,  [sp, #32]
    stp x6,  x7,  [sp, #48]
    stp x8,  x9,  [sp, #64]
    stp x10, x11, [sp, #80]
    stp x12, x13, [sp, #96]
    stp x14, x15, [sp, #112]
    stp x16, x17, [sp, #128]
    stp x18, x19, [sp, #144]
    stp x20, x21, [sp, #160]
    stp x22, x23, [sp, #176]
    stp x24, x25, [sp, #192]
    stp x26, x27, [sp, #208]
    stp x28, x29, [sp, #224]
    mrs x0, elr_el1
    stp x30, x0,  [sp, #240]
    mrs x0, spsr_el1
    str x0,  [sp, #256]
.endm

.macro kernel_exit
    ldr x0,  [sp, #256]
    msr spsr_el1, x0
    ldp x30, x0,  [sp, #240]
    msr elr_el1, x0
    ldp x0,  x1,  [sp, #0]
    ldp x2,  x3,  [sp, #16]
    ldp x4,  x5,  [sp, #32]
    ldp x6,  x7,  [sp, #48]
    ldp x8,  x9,  [sp, #64]
    ldp x10, x11, [sp, #80]
    ldp x12, x13, [sp, #96]
    ldp x14, x15, [sp, #112]
    ldp x16, x17, [sp, #128]
    ldp x18, x19, [sp, #144]
    ldp x20, x21, [sp, #160]
    ldp x22, x23, [sp, #176]
    ldp x24, x25, [sp, #192]
    ldp x26, x27, [sp, #208]
    ldp x28, x29, [sp, #224]
    add sp, sp, #272
    eret
.endm

.section ".text"
.align 11                       /* VBAR_EL1 requires 2KB alignment */
.global vector_table
vector_table:
    vector_stub                 /* sync,  current EL, SP0 */
    vector_stub                 /* irq,   current EL, SP0 */
    vector_stub                 /* fiq,   current EL, SP0 */
    vector_stub                 /* serror,current EL, SP0 */
    vector_stub                 /* sync,  current EL, SPx */

    /* irq, current EL, SPx — the only real handler for now. Each
     * vector slot is hard-limited to 32 instructions (0x80 bytes), so
     * the full save/dispatch/restore sequence has to live out of
     * line — this slot only branches to it. */
    b irq_common_handler
    .balign 0x80

    vector_stub                 /* fiq,   current EL, SPx */
    vector_stub                 /* serror,current EL, SPx */

    b svc_common_handler        /* sync,  lower EL, aarch64 — EL0 syscalls */
    .balign 0x80

    b irq_common_handler        /* irq,   lower EL, aarch64 — timer preempting EL0 */
    .balign 0x80

    vector_stub                 /* fiq,   lower EL, aarch64 */
    vector_stub                 /* serror,lower EL, aarch64 */
    vector_stub                 /* sync,  lower EL, aarch32 */
    vector_stub                 /* irq,   lower EL, aarch32 */
    vector_stub                 /* fiq,   lower EL, aarch32 */
    vector_stub                 /* serror,lower EL, aarch32 */

unhandled_exception:
    wfe
    b unhandled_exception

irq_common_handler:
    kernel_entry
    bl aarch64_irq_handler
    kernel_exit

svc_common_handler:
    kernel_entry
    mov x0, sp
    bl aarch64_svc_handler
    kernel_exit
