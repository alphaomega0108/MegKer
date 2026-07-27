/* include/arch/x86_64/mm.h
 * x86_64 memory init — parses the Multiboot2 info blob GRUB hands us
 * and feeds the result to the arch-independent PMM.
 */

#ifndef ARCH_X86_64_MM_H
#define ARCH_X86_64_MM_H

#include <kernel/types.h>

void x86_64_mm_init(u64 boot_magic, void* boot_info);
u64  x86_64_total_ram(void);

#endif /* ARCH_X86_64_MM_H */
