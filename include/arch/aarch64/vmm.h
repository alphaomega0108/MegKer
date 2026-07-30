/* include/arch/aarch64/vmm.h
 * Dynamic per-address-space VMM, built on top of the static boot
 * identity map (see mm/mmu_boot.c). Same shape as x86_64's VMM:
 * arbitrary virt != phys 4KB mappings, and separate address spaces
 * that still share the kernel's own mappings (so EL1 code — the
 * boot thread, interrupts — stays reachable no matter which address
 * space is active). No user/kernel split enforced yet; that lands
 * with EL0 userspace.
 *
 * 4KB granule, 3-level translation (L1 starts the walk, 39-bit VA —
 * see mmu_boot.c). vmm_map()/vmm_create_address_space() always
 * terminate at L3 (4KB pages); the two 1GB L1 *block* entries the
 * boot map installs are leaves handled the same way x86_64 handles
 * its boot huge pages — aliased whole, never split, never descended
 * into by vmm_map() (which safely no-ops if a request lands there).
 */

#ifndef ARCH_AARCH64_VMM_H
#define ARCH_AARCH64_VMM_H

#include <kernel/types.h>

#define VMM_PRESENT  (1ULL << 0)
#define VMM_WRITABLE (1ULL << 1)   /* arch-independent-looking flag name; see AP encoding in vmm.c */
#define VMM_USER     (1ULL << 2)

typedef struct {
    physaddr l1_phys;
} address_space_t;

void vmm_init(void);   /* captures the current (boot-time) TTBR0_EL1 as the kernel space */

address_space_t* vmm_kernel_space(void);
address_space_t* vmm_create_address_space(void);
void vmm_destroy_address_space(address_space_t* as);

void vmm_map(address_space_t* as, virtaddr virt, physaddr phys, u64 flags);
void vmm_unmap(address_space_t* as, virtaddr virt);
physaddr vmm_translate(address_space_t* as, virtaddr virt);   /* 0 if unmapped */

void vmm_switch(address_space_t* as);

#endif /* ARCH_AARCH64_VMM_H */
