/* include/arch/x86_64/idt.h
 * Interrupt Descriptor Table for x86_64.
 */

#ifndef ARCH_X86_64_IDT_H
#define ARCH_X86_64_IDT_H

#include <kernel/types.h>

#define IDT_ENTRIES  256

/* PIC IRQs are remapped here so they don't collide with the
 * CPU exception vectors (0-31). */
#define IRQ_BASE     32
#define IRQ_COUNT    16

/* Register snapshot built by isr_common_stub (arch/x86_64/boot/isr_stubs.asm).
 * Field order must match the push order there exactly. */
typedef struct {
    u64 r15, r14, r13, r12, r11, r10, r9, r8;
    u64 rbp, rdi, rsi, rdx, rcx, rbx, rax;
    u64 int_no, err_code;
    u64 rip, cs, rflags, user_rsp, ss;
} registers_t;

typedef void (*irq_handler_t)(registers_t* regs);

void idt_init(void);
void irq_install_handler(int irq, irq_handler_t handler);
void irq_set_mask(int irq, bool masked);

#endif /* ARCH_X86_64_IDT_H */
