/* include/arch/x86_64/vmm.h
 * Virtual memory manager: arbitrary virt != phys 4KB mappings, and
 * separate address spaces (own PML4) that still share the kernel's
 * own mappings — needed so ring 0 (interrupts, and later syscalls)
 * stays reachable no matter which address space is active.
 *
 * There's no user/kernel split yet (no userspace exists yet either)
 * — every new address space starts as a full copy of the kernel's
 * top-level mappings. That split is expected to land alongside
 * userspace support.
 */

#ifndef ARCH_X86_64_VMM_H
#define ARCH_X86_64_VMM_H

#include <kernel/types.h>

#define VMM_PRESENT  (1ULL << 0)
#define VMM_WRITABLE (1ULL << 1)
#define VMM_USER     (1ULL << 2)
#define VMM_HUGE     (1ULL << 7)   /* PD/PDPT entry is a leaf (2MB/1GB page), not a child table */

typedef struct {
    physaddr pml4_phys;
} address_space_t;

void vmm_init(void);   /* captures the current (boot-time) CR3 as the kernel space */

address_space_t* vmm_kernel_space(void);
address_space_t* vmm_create_address_space(void);
void vmm_destroy_address_space(address_space_t* as);

void vmm_map(address_space_t* as, virtaddr virt, physaddr phys, u64 flags);
void vmm_unmap(address_space_t* as, virtaddr virt);
physaddr vmm_translate(address_space_t* as, virtaddr virt);   /* 0 if unmapped */

void vmm_switch(address_space_t* as);

#endif /* ARCH_X86_64_VMM_H */
