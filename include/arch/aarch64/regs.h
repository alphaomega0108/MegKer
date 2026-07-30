/* include/arch/aarch64/regs.h
 * Layout of the register frame boot/exceptions.S's kernel_entry macro
 * pushes — field order/size has to match that macro exactly (see the
 * comment there for the offset table).
 */

#ifndef ARCH_AARCH64_REGS_H
#define ARCH_AARCH64_REGS_H

#include <kernel/types.h>

typedef struct {
    u64 x0,  x1,  x2,  x3,  x4,  x5,  x6,  x7;
    u64 x8,  x9,  x10, x11, x12, x13, x14, x15;
    u64 x16, x17, x18, x19, x20, x21, x22, x23;
    u64 x24, x25, x26, x27, x28, x29;
    u64 x30, elr;
    u64 spsr;
    u64 _pad;
} aarch64_regs_t;

#endif /* ARCH_AARCH64_REGS_H */
