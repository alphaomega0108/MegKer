/* arch/x86_64/mm/paging.c
 * See include/arch/x86_64/paging.h for scope/limits.
 *
 * Walks the live PML4 (read from CR3 — physical == virtual for
 * everything involved, since we never leave identity mapping)
 * and fills in whatever PDP/PD entries are missing with freshly
 * zeroed tables from the PMM, then maps the requested range with
 * present+writable 2MB pages.
 */

#include <arch/x86_64/paging.h>
#include <mm/pmm.h>

#define PAGE_PRESENT  (1ULL << 0)
#define PAGE_WRITABLE (1ULL << 1)
#define PAGE_HUGE     (1ULL << 7)
#define PAGE_ADDR_MASK 0x000FFFFFFFFFF000ULL

static inline u64* get_pml4(void)
{
    u64 cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    return (u64*)(cr3 & PAGE_ADDR_MASK);
}

static u64* ensure_table(u64* parent, u32 index)
{
    if (!(parent[index] & PAGE_PRESENT)) {
        physaddr frame = pmm_alloc_frame();
        u64* table = (u64*)frame;
        for (int i = 0; i < 512; i++)
            table[i] = 0;
        parent[index] = frame | PAGE_PRESENT | PAGE_WRITABLE;
    }
    return (u64*)(parent[index] & PAGE_ADDR_MASK);
}

void paging_identity_map(physaddr phys, u64 size)
{
    physaddr start = ALIGN_DOWN(phys, 0x200000ULL);
    physaddr end    = ALIGN_UP(phys + size, 0x200000ULL);

    u64* pml4 = get_pml4();

    for (physaddr addr = start; addr < end; addr += 0x200000ULL) {
        u32 pml4_i = (addr >> 39) & 0x1FF;
        u32 pdp_i  = (addr >> 30) & 0x1FF;
        u32 pd_i   = (addr >> 21) & 0x1FF;

        u64* pdp = ensure_table(pml4, pml4_i);
        u64* pd  = ensure_table(pdp, pdp_i);

        if (!(pd[pd_i] & PAGE_PRESENT))
            pd[pd_i] = addr | PAGE_PRESENT | PAGE_WRITABLE | PAGE_HUGE;
    }

    /* Reload CR3 with itself — simplest correct way to flush the TLB
     * for the new mappings; this runs at init time, not a hot path. */
    u64 cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    __asm__ volatile ("mov %0, %%cr3" :: "r"(cr3) : "memory");
}
