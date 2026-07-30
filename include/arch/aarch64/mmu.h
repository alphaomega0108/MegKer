/* include/arch/aarch64/mmu.h
 * Raw MMU bring-up — builds a static, pre-PMM boot identity map and
 * turns the MMU on. Runs before pmm/heap exist (see mm/mmu_boot.c);
 * the dynamic per-address-space VMM (vmm.h) builds on top of this
 * once those are available.
 */

#ifndef ARCH_AARCH64_MMU_H
#define ARCH_AARCH64_MMU_H

void aarch64_mmu_init(void);

#endif /* ARCH_AARCH64_MMU_H */
