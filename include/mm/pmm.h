/* include/mm/pmm.h
 * Physical memory manager — a bitmap frame allocator.
 * Arch-independent: arch code discovers RAM (however its bootloader
 * describes it) and hands the result here as a plain region list.
 */

#ifndef MM_PMM_H
#define MM_PMM_H

#include <kernel/types.h>

#define PMM_FRAME_SIZE 4096

typedef struct {
    physaddr base;
    u64      length;
} mem_region_t;

/* regions:        usable RAM, as reported by the bootloader
 * reserved_start/_end: physical range to keep marked used regardless
 *                  (the kernel image itself — the bitmap is placed
 *                  right after it and reserves itself too) */
void pmm_init(const mem_region_t* regions, u32 region_count,
              physaddr reserved_start, physaddr reserved_end);

physaddr pmm_alloc_frame(void);
void     pmm_free_frame(physaddr addr);

/* Like pmm_alloc_frame(), but returns `count` physically contiguous
 * frames (needed by callers — like the kernel heap — that do pointer
 * arithmetic across the whole span, not just frame-at-a-time bookkeeping). */
physaddr pmm_alloc_frames(u64 count);
void     pmm_free_frames(physaddr addr, u64 count);

u64 pmm_total_frames(void);
u64 pmm_free_frame_count(void);

#endif /* MM_PMM_H */
