/* include/arch/x86_64/paging.h
 * Minimal on-demand identity-mapping — extends the kernel's existing
 * page tables (still the ones boot.asm built) to cover physical
 * ranges beyond the initial 1GB, for devices like the framebuffer
 * whose MMIO lives well above that.
 *
 * This is NOT a general virtual memory manager: everything stays
 * identity-mapped (virt == phys), and there's no per-address-space
 * page tables yet — that's a separate, later piece of work needed
 * once userspace processes exist.
 */

#ifndef ARCH_X86_64_PAGING_H
#define ARCH_X86_64_PAGING_H

#include <kernel/types.h>

/* Ensures [phys, phys+size) is present + writable in the current
 * page tables, identity-mapped, using 2MB pages. */
void paging_identity_map(physaddr phys, u64 size);

#endif /* ARCH_X86_64_PAGING_H */
