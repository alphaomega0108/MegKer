/* arch/aarch64/boot/exceptions.S
 * Minimal AArch64 exception vector table. arch_interrupts_enable()
 * unmasks DAIF unconditionally (kernel_main() calls it on every
 * arch), so VBAR_EL1 must point somewhere sane before that happens —
 * without a real GIC/timer wired up yet (see arch.c), every entry
 * just halts rather than silently executing whatever garbage sits at
 * address 0.
 */

.macro vector_stub
    b unhandled_exception
    .balign 0x80
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
    vector_stub                 /* irq,   current EL, SPx */
    vector_stub                 /* fiq,   current EL, SPx */
    vector_stub                 /* serror,current EL, SPx */
    vector_stub                 /* sync,  lower EL, aarch64 */
    vector_stub                 /* irq,   lower EL, aarch64 */
    vector_stub                 /* fiq,   lower EL, aarch64 */
    vector_stub                 /* serror,lower EL, aarch64 */
    vector_stub                 /* sync,  lower EL, aarch32 */
    vector_stub                 /* irq,   lower EL, aarch32 */
    vector_stub                 /* fiq,   lower EL, aarch32 */
    vector_stub                 /* serror,lower EL, aarch32 */

unhandled_exception:
    wfe
    b unhandled_exception
