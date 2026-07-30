/* arch/aarch64/mm/vmm.c
 * See include/arch/aarch64/vmm.h for scope. AArch64 4KB-granule,
 * 3-level translation (L1 -> L2 -> L3, 39-bit VA — matches the TCR_EL1
 * setup in mm/mmu_boot.c).
 */

#include <arch/aarch64/vmm.h>
#include <mm/pmm.h>
#include <mm/heap.h>

#define ADDR_MASK 0x0000FFFFFFFFF000ULL

#define PTE_VALID (1ULL << 0)
#define PTE_TABLE (1ULL << 1)   /* L1/L2: table pointer if set, else a block leaf. L3: always set for a valid page. */
#define PTE_AF    (1ULL << 10)
#define PTE_SH_INNER (3ULL << 8)

/* AP[2:1] (descriptor bits [7:6]): 00=RW@EL1 only, 01=RW@EL1+EL0,
 * 10=RO@EL1 only, 11=RO@EL1+EL0. */
#define PTE_AP_RW_EL1      (0ULL << 6)
#define PTE_AP_RW_EL1_EL0  (1ULL << 6)
#define PTE_AP_RO_EL1      (2ULL << 6)
#define PTE_AP_RO_EL1_EL0  (3ULL << 6)

#define MAIR_NORMAL_IDX 1   /* matches mmu_boot.c's MAIR_EL1 layout */
#define PTE_ATTR_NORMAL (MAIR_NORMAL_IDX << 2)

static address_space_t kernel_space;

static u64* zeroed_table(physaddr frame)
{
    u64* table = (u64*)frame;
    for (int i = 0; i < 512; i++)
        table[i] = 0;
    return table;
}

static u64 leaf_flags(u64 flags)
{
    u64 ap;
    bool user  = flags & VMM_USER;
    bool write = flags & VMM_WRITABLE;

    if (user)
        ap = write ? PTE_AP_RW_EL1_EL0 : PTE_AP_RO_EL1_EL0;
    else
        ap = write ? PTE_AP_RW_EL1 : PTE_AP_RO_EL1;

    return PTE_VALID | PTE_TABLE | PTE_AF | PTE_SH_INNER | PTE_ATTR_NORMAL | ap;
}

static u64* get_or_create_table(u64* parent, u32 index)
{
    if ((parent[index] & PTE_VALID) && !(parent[index] & PTE_TABLE))
        return NULL;   /* a block leaf here isn't a child table — can't descend */

    if (!(parent[index] & PTE_VALID)) {
        physaddr frame = pmm_alloc_frame();
        zeroed_table(frame);
        parent[index] = frame | PTE_VALID | PTE_TABLE;
    }
    return (u64*)(parent[index] & ADDR_MASK);
}

/* Recursively duplicates page-table structure (new frames at every
 * level) while aliasing the same underlying data/block leaves — a
 * clone can gain new mappings without ever mutating a table the
 * source address space still uses. level: 1=L1, 2=L2, 3=L3. */
static u64* clone_table_level(const u64* src, int level)
{
    physaddr new_frame = pmm_alloc_frame();
    u64* dst = zeroed_table(new_frame);

    for (int i = 0; i < 512; i++) {
        if (!(src[i] & PTE_VALID))
            continue;

        bool leaf = (level == 3) || !(src[i] & PTE_TABLE);
        if (leaf) {
            dst[i] = src[i];   /* alias the same physical block/page */
            continue;
        }

        const u64* child_src = (const u64*)(src[i] & ADDR_MASK);
        u64* child_dst = clone_table_level(child_src, level + 1);
        dst[i] = ((physaddr)(usize)child_dst & ADDR_MASK) | PTE_VALID | PTE_TABLE;
    }
    return dst;
}

/* Mirror of clone_table_level: frees every table frame this address
 * space owns, but never a block/page leaf's data frame — those are
 * shared, not owned. */
static void free_table_level(u64* table, int level)
{
    if (level < 3) {
        for (int i = 0; i < 512; i++) {
            if ((table[i] & PTE_VALID) && (table[i] & PTE_TABLE))
                free_table_level((u64*)(table[i] & ADDR_MASK), level + 1);
        }
    }
    pmm_free_frame((physaddr)(usize)table);
}

void vmm_init(void)
{
    u64 ttbr0;
    __asm__ volatile ("mrs %0, ttbr0_el1" : "=r" (ttbr0));
    kernel_space.l1_phys = ttbr0 & ADDR_MASK;
}

address_space_t* vmm_kernel_space(void)
{
    return &kernel_space;
}

address_space_t* vmm_create_address_space(void)
{
    u64* kernel_l1 = (u64*)kernel_space.l1_phys;
    u64* new_l1 = clone_table_level(kernel_l1, 1);

    address_space_t* as = (address_space_t*)kmalloc(sizeof(address_space_t));
    as->l1_phys = (physaddr)(usize)new_l1;
    return as;
}

void vmm_destroy_address_space(address_space_t* as)
{
    if (as == &kernel_space)
        return;
    free_table_level((u64*)as->l1_phys, 1);
    kfree(as);
}

/* NOTE: none of vmm_map/vmm_unmap/vmm_translate split an existing
 * 1GB block — a request that lands inside one is safely ignored
 * (map) or treated as unmapped (unmap/translate) rather than
 * misreading the block's physical address as a child table pointer.
 * Fine-grained mappings within the boot identity map's block range
 * aren't supported yet (same limitation x86_64 has for its huge
 * pages). */

void vmm_map(address_space_t* as, virtaddr virt, physaddr phys, u64 flags)
{
    u64* l1 = (u64*)as->l1_phys;
    u32 i1 = (virt >> 30) & 0x1FF;
    u32 i2 = (virt >> 21) & 0x1FF;
    u32 i3 = (virt >> 12) & 0x1FF;

    u64* l2 = get_or_create_table(l1, i1);
    if (!l2) return;
    u64* l3 = get_or_create_table(l2, i2);
    if (!l3) return;

    if (!(flags & VMM_PRESENT)) return;
    l3[i3] = (phys & ADDR_MASK) | leaf_flags(flags);
}

void vmm_unmap(address_space_t* as, virtaddr virt)
{
    u64* l1 = (u64*)as->l1_phys;
    u32 i1 = (virt >> 30) & 0x1FF;
    u32 i2 = (virt >> 21) & 0x1FF;
    u32 i3 = (virt >> 12) & 0x1FF;

    if (!(l1[i1] & PTE_VALID) || !(l1[i1] & PTE_TABLE)) return;
    u64* l2 = (u64*)(l1[i1] & ADDR_MASK);
    if (!(l2[i2] & PTE_VALID) || !(l2[i2] & PTE_TABLE)) return;
    u64* l3 = (u64*)(l2[i2] & ADDR_MASK);

    l3[i3] = 0;
    __asm__ volatile ("dsb ishst; tlbi vaae1is, %0; dsb ish; isb" :: "r" (virt >> 12) : "memory");
}

physaddr vmm_translate(address_space_t* as, virtaddr virt)
{
    u64* l1 = (u64*)as->l1_phys;
    u32 i1 = (virt >> 30) & 0x1FF;
    u32 i2 = (virt >> 21) & 0x1FF;
    u32 i3 = (virt >> 12) & 0x1FF;

    if (!(l1[i1] & PTE_VALID) || !(l1[i1] & PTE_TABLE)) return 0;
    u64* l2 = (u64*)(l1[i1] & ADDR_MASK);
    if (!(l2[i2] & PTE_VALID) || !(l2[i2] & PTE_TABLE)) return 0;
    u64* l3 = (u64*)(l2[i2] & ADDR_MASK);
    if (!(l3[i3] & PTE_VALID)) return 0;

    return (l3[i3] & ADDR_MASK) | (virt & 0xFFF);
}

void vmm_switch(address_space_t* as)
{
    __asm__ volatile (
        "msr ttbr0_el1, %0\n"
        "isb\n"
        "tlbi vmalle1is\n"
        "dsb ish\n"
        "isb\n"
        :: "r" (as->l1_phys) : "memory"
    );
}
