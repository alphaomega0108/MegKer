/* mm/pmm.c
 * Bitmap physical frame allocator. One bit per 4KB frame: 1 = used,
 * 0 = free. The bitmap itself is placed right after the kernel image
 * (reserved_end) — there's no heap yet, so it can't come from anywhere
 * else, and the kernel's identity mapping already covers that memory.
 */

#include <mm/pmm.h>

static u8* bitmap;
static u64 bitmap_bytes;
static u64 total_frames;
static u64 free_frames;

static inline void bit_set(u64 frame)
{
    bitmap[frame / 8] |= (u8)(1 << (frame % 8));
}

static inline void bit_clear(u64 frame)
{
    bitmap[frame / 8] &= (u8)~(1 << (frame % 8));
}

static inline bool bit_test(u64 frame)
{
    return (bitmap[frame / 8] >> (frame % 8)) & 1;
}

static void mark_range(physaddr start, physaddr end, bool used)
{
    u64 first = start / PMM_FRAME_SIZE;
    u64 last  = ALIGN_DOWN(end, PMM_FRAME_SIZE) / PMM_FRAME_SIZE;

    for (u64 f = first; f < last && f < total_frames; f++) {
        if (used) {
            if (!bit_test(f)) { bit_set(f); free_frames--; }
        } else {
            if (bit_test(f)) { bit_clear(f); free_frames++; }
        }
    }
}

void pmm_init(const mem_region_t* regions, u32 region_count,
              physaddr reserved_start, physaddr reserved_end)
{
    physaddr highest = 0;
    for (u32 i = 0; i < region_count; i++) {
        physaddr top = regions[i].base + regions[i].length;
        if (top > highest)
            highest = top;
    }

    total_frames = highest / PMM_FRAME_SIZE;
    bitmap_bytes = (total_frames + 7) / 8;

    /* Bitmap lives right after whatever the caller says is reserved
     * (the kernel image) — and is itself part of the reservation. */
    bitmap = (u8*)ALIGN_UP(reserved_end, 8);
    physaddr bitmap_end = (physaddr)bitmap + bitmap_bytes;

    for (u64 i = 0; i < bitmap_bytes; i++)
        bitmap[i] = 0xFF;   /* everything used until proven free */
    free_frames = 0;

    for (u32 i = 0; i < region_count; i++)
        mark_range(regions[i].base, regions[i].base + regions[i].length, false);

    mark_range(reserved_start, bitmap_end, true);
}

physaddr pmm_alloc_frame(void)
{
    for (u64 f = 0; f < total_frames; f++) {
        if (!bit_test(f)) {
            bit_set(f);
            free_frames--;
            return f * PMM_FRAME_SIZE;
        }
    }
    return 0;   /* out of memory — 0 is never a valid handed-out frame */
}

void pmm_free_frame(physaddr addr)
{
    u64 f = addr / PMM_FRAME_SIZE;
    if (f >= total_frames || !bit_test(f))
        return;

    bit_clear(f);
    free_frames++;
}

physaddr pmm_alloc_frames(u64 count)
{
    if (count == 0)
        return 0;

    u64 run_start = 0;
    u64 run_len   = 0;

    for (u64 f = 0; f < total_frames; f++) {
        if (bit_test(f)) {
            run_len = 0;
            continue;
        }
        if (run_len == 0)
            run_start = f;
        run_len++;

        if (run_len == count) {
            for (u64 i = run_start; i < run_start + count; i++)
                bit_set(i);
            free_frames -= count;
            return run_start * PMM_FRAME_SIZE;
        }
    }
    return 0;   /* no run big enough */
}

void pmm_free_frames(physaddr addr, u64 count)
{
    u64 first = addr / PMM_FRAME_SIZE;
    for (u64 f = first; f < first + count && f < total_frames; f++) {
        if (bit_test(f)) { bit_clear(f); free_frames++; }
    }
}

u64 pmm_total_frames(void)     { return total_frames; }
u64 pmm_free_frame_count(void) { return free_frames; }
