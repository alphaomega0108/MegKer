/* arch/x86_64/mm/vmm.c
 * See include/arch/x86_64/vmm.h for scope. Standard 4-level x86_64
 * paging (PML4 -> PDPT -> PD -> PT), 4KB pages throughout.
 */

#include <arch/x86_64/vmm.h>
#include <mm/pmm.h>
#include <mm/heap.h>

#define ADDR_MASK 0x000FFFFFFFFFF000ULL

static address_space_t kernel_space;

static u64* zeroed_table(physaddr frame)
{
    u64* table = (u64*)frame;
    for (int i = 0; i < 512; i++)
        table[i] = 0;
    return table;
}

static u64* get_or_create_table(u64* parent, u32 index, u64 flags)
{
    if (parent[index] & VMM_HUGE)
        return NULL;   /* a huge-page leaf here isn't a child table — can't descend */

    if (!(parent[index] & VMM_PRESENT)) {
        physaddr frame = pmm_alloc_frame();
        zeroed_table(frame);
        parent[index] = frame | VMM_PRESENT | VMM_WRITABLE | (flags & VMM_USER);
    } else if (flags & VMM_USER) {
        parent[index] |= VMM_USER;   /* widen so a user mapping further down is reachable */
    }
    return (u64*)(parent[index] & ADDR_MASK);
}

/* Recursively duplicates page-table structure (new frames at every
 * level) while aliasing the same underlying data frames at the
 * leaves — so a clone can gain new mappings without ever mutating a
 * table the source address space still uses. level: 4=PML4, 3=PDPT,
 * 2=PD, 1=PT. Huge-page entries (PD/PDPT) are leaves too — their
 * "address" is data, not a child table pointer. */
static u64* clone_table_level(const u64* src, int level)
{
    physaddr new_frame = pmm_alloc_frame();
    u64* dst = zeroed_table(new_frame);

    for (int i = 0; i < 512; i++) {
        if (!(src[i] & VMM_PRESENT))
            continue;

        bool leaf = (level == 1) || (src[i] & VMM_HUGE);
        if (leaf) {
            dst[i] = src[i];   /* alias the same physical data */
            continue;
        }

        const u64* child_src = (const u64*)(src[i] & ADDR_MASK);
        u64* child_dst = clone_table_level(child_src, level - 1);
        dst[i] = ((physaddr)(usize)child_dst & ADDR_MASK) | (src[i] & 0xFFFULL);
    }
    return dst;
}

/* Mirror of clone_table_level: frees every table frame this address
 * space owns (levels 4-1), but never the leaf data frames a PT/huge
 * entry points to — those are shared, not owned. */
static void free_table_level(u64* table, int level)
{
    if (level > 1) {
        for (int i = 0; i < 512; i++) {
            if ((table[i] & VMM_PRESENT) && !(table[i] & VMM_HUGE))
                free_table_level((u64*)(table[i] & ADDR_MASK), level - 1);
        }
    }
    pmm_free_frame((physaddr)(usize)table);
}

void vmm_init(void)
{
    u64 cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    kernel_space.pml4_phys = cr3 & ADDR_MASK;
}

address_space_t* vmm_kernel_space(void)
{
    return &kernel_space;
}

address_space_t* vmm_create_address_space(void)
{
    u64* kernel_pml4 = (u64*)kernel_space.pml4_phys;
    u64* new_pml4 = clone_table_level(kernel_pml4, 4);

    address_space_t* as = (address_space_t*)kmalloc(sizeof(address_space_t));
    as->pml4_phys = (physaddr)(usize)new_pml4;
    return as;
}

void vmm_destroy_address_space(address_space_t* as)
{
    if (as == &kernel_space)
        return;
    free_table_level((u64*)as->pml4_phys, 4);
    kfree(as);
}

/* NOTE: none of vmm_map/vmm_unmap/vmm_translate split an existing
 * huge page — a request that lands inside one is safely ignored
 * (map) or treated as unmapped (unmap/translate) rather than
 * misreading the huge entry's physical address as a child table
 * pointer. Fine-grained mappings within the boot identity map's
 * huge-page range aren't supported yet. */

void vmm_map(address_space_t* as, virtaddr virt, physaddr phys, u64 flags)
{
    u64* pml4 = (u64*)as->pml4_phys;
    u32 i4 = (virt >> 39) & 0x1FF;
    u32 i3 = (virt >> 30) & 0x1FF;
    u32 i2 = (virt >> 21) & 0x1FF;
    u32 i1 = (virt >> 12) & 0x1FF;

    u64* pdpt = get_or_create_table(pml4, i4, flags);
    if (!pdpt) return;
    u64* pd = get_or_create_table(pdpt, i3, flags);
    if (!pd) return;
    u64* pt = get_or_create_table(pd, i2, flags);
    if (!pt) return;

    pt[i1] = (phys & ADDR_MASK) | (flags & (VMM_PRESENT | VMM_WRITABLE | VMM_USER));
}

void vmm_unmap(address_space_t* as, virtaddr virt)
{
    u64* pml4 = (u64*)as->pml4_phys;
    u32 i4 = (virt >> 39) & 0x1FF;
    u32 i3 = (virt >> 30) & 0x1FF;
    u32 i2 = (virt >> 21) & 0x1FF;
    u32 i1 = (virt >> 12) & 0x1FF;

    if (!(pml4[i4] & VMM_PRESENT) || (pml4[i4] & VMM_HUGE)) return;
    u64* pdpt = (u64*)(pml4[i4] & ADDR_MASK);
    if (!(pdpt[i3] & VMM_PRESENT) || (pdpt[i3] & VMM_HUGE)) return;
    u64* pd = (u64*)(pdpt[i3] & ADDR_MASK);
    if (!(pd[i2] & VMM_PRESENT) || (pd[i2] & VMM_HUGE)) return;
    u64* pt = (u64*)(pd[i2] & ADDR_MASK);

    pt[i1] = 0;
    __asm__ volatile ("invlpg (%0)" :: "r"(virt) : "memory");
}

physaddr vmm_translate(address_space_t* as, virtaddr virt)
{
    u64* pml4 = (u64*)as->pml4_phys;
    u32 i4 = (virt >> 39) & 0x1FF;
    u32 i3 = (virt >> 30) & 0x1FF;
    u32 i2 = (virt >> 21) & 0x1FF;
    u32 i1 = (virt >> 12) & 0x1FF;

    if (!(pml4[i4] & VMM_PRESENT) || (pml4[i4] & VMM_HUGE)) return 0;
    u64* pdpt = (u64*)(pml4[i4] & ADDR_MASK);
    if (!(pdpt[i3] & VMM_PRESENT) || (pdpt[i3] & VMM_HUGE)) return 0;
    u64* pd = (u64*)(pdpt[i3] & ADDR_MASK);
    if (!(pd[i2] & VMM_PRESENT) || (pd[i2] & VMM_HUGE)) return 0;
    u64* pt = (u64*)(pd[i2] & ADDR_MASK);
    if (!(pt[i1] & VMM_PRESENT)) return 0;

    return (pt[i1] & ADDR_MASK) | (virt & 0xFFF);
}

void vmm_switch(address_space_t* as)
{
    __asm__ volatile ("mov %0, %%cr3" :: "r"(as->pml4_phys) : "memory");
}
