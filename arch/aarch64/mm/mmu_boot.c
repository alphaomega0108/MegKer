/* arch/aarch64/mm/mmu_boot.c
 * Turns the MMU on with a coarse, static identity map — this has to
 * run before the PMM/heap exist (it's called from arch_early_init(),
 * before arch_mm_init()), so the L1 table lives in a fixed .bss
 * array rather than a PMM-allocated frame, the same reason x86_64's
 * boot.asm builds its own page tables directly rather than waiting
 * for the PMM.
 *
 * QEMU's `virt` machine layout makes this unusually simple: 4KB
 * granule, 3-level translation (L1 starts the walk — 39-bit VA), and
 * exactly two 1GB L1 block entries cover everything the kernel
 * currently touches:
 *   - [0, 1GB)  — GICD/GICC/UART and everything else QEMU puts in
 *                 the low MMIO range; nothing here is RAM.
 *   - [1GB, 2GB) — all of AARCH64_RAM_SIZE (see arch.c), including
 *                 the kernel image itself.
 * arch/aarch64/mm/vmm.c builds the *dynamic* per-address-space VMM
 * on top of this once pmm/heap exist; it clones these same two block
 * entries into every new address space exactly like x86_64 aliases
 * its boot huge pages.
 */

#include <arch/aarch64/mmu.h>
#include <kernel/types.h>

#define MAIR_DEVICE_IDX 0
#define MAIR_NORMAL_IDX 1

#define MAIR_DEVICE_NGNRNE 0x00ULL
#define MAIR_NORMAL_WB     0xFFULL

#define PTE_VALID    (1ULL << 0)
#define PTE_AF       (1ULL << 10)
#define PTE_SH_INNER (3ULL << 8)
#define PTE_AP_RW_EL1_ONLY (0ULL << 6)
#define PTE_ATTR(idx) ((u64)(idx) << 2)
#define PTE_PXN      (1ULL << 53)
#define PTE_UXN      (1ULL << 54)

/* Static, page-aligned, zero-initialized by the .bss clear in
 * boot.S — already zero by the time any C code (including this
 * function) runs. */
static u64 boot_l1_table[512] __attribute__((aligned(4096)));

void aarch64_mmu_init(void)
{
    boot_l1_table[0] = 0x00000000ULL | PTE_VALID | PTE_AF | PTE_SH_INNER |
                        PTE_AP_RW_EL1_ONLY | PTE_ATTR(MAIR_DEVICE_IDX) | PTE_PXN | PTE_UXN;

    boot_l1_table[1] = 0x40000000ULL | PTE_VALID | PTE_AF | PTE_SH_INNER |
                        PTE_AP_RW_EL1_ONLY | PTE_ATTR(MAIR_NORMAL_IDX);
    /* PXN left clear here — the kernel's own code lives in this 1GB
     * block and has to stay executable at EL1. */

    u64 mair = (MAIR_NORMAL_WB << (8 * MAIR_NORMAL_IDX)) |
               (MAIR_DEVICE_NGNRNE << (8 * MAIR_DEVICE_IDX));
    __asm__ volatile ("msr mair_el1, %0" :: "r" (mair));

    u64 tcr = 25ULL              /* T0SZ — 39-bit VA (2^39 = 512GB), matches L1-starts-the-walk */
            | (1ULL << 8)        /* IRGN0 — Normal WB, write-allocate */
            | (1ULL << 10)       /* ORGN0 — Normal WB, write-allocate */
            | (3ULL << 12)       /* SH0   — inner shareable */
            | (0ULL << 14)       /* TG0   — 4KB granule */
            | (1ULL << 23)       /* EPD1  — no TTBR1 walks, we don't use it */
            | (1ULL << 32);      /* IPS   — 36-bit PA (64GB), comfortably covers QEMU virt */
    __asm__ volatile ("msr tcr_el1, %0" :: "r" (tcr));

    __asm__ volatile ("msr ttbr0_el1, %0" :: "r" ((u64)(usize)boot_l1_table));

    __asm__ volatile ("isb");

    u64 sctlr;
    __asm__ volatile ("mrs %0, sctlr_el1" : "=r" (sctlr));
    sctlr |= (1ULL << 0)   /* M — MMU enable */
           | (1ULL << 2)   /* C — data cache enable */
           | (1ULL << 12); /* I — instruction cache enable */
    __asm__ volatile ("msr sctlr_el1, %0" :: "r" (sctlr));

    __asm__ volatile ("isb");
}
