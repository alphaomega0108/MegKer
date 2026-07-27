/* arch/x86_64/mm/multiboot2.c
 * Walks the Multiboot2 info structure GRUB passes in (magic in the
 * first kernel_main arg, info pointer in the second — see boot.asm's
 * long_mode_entry) to find the memory map tag, then hands the usable
 * regions to the arch-independent PMM.
 */

#include <kernel/types.h>
#include <arch/x86_64/mm.h>
#include <mm/pmm.h>

#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36d76289u

#define MB2_TAG_END   0
#define MB2_TAG_MMAP  6
#define MB2_MEM_AVAILABLE 1

/* Our boot.asm sets up 2MB identity-mapped pages for the first 1GB
 * only (see setup_page_tables) — anything past that isn't mapped yet,
 * so we can't safely hand it out as allocatable RAM. */
#define IDENTITY_MAP_LIMIT 0x40000000ULL
#define LOW_MEM_RESERVED   0x100000ULL   /* BIOS/EBDA/VGA area below 1MB */

typedef struct {
    u32 type;
    u32 size;
} mb2_tag_t;

typedef struct {
    u32 type, size, entry_size, entry_version;
} mb2_tag_mmap_t;

typedef struct {
    u64 addr;
    u64 len;
    u32 type;
    u32 reserved;
} mb2_mmap_entry_t;

extern u8 _kernel_start[];
extern u8 _kernel_end[];

static u64 total_ram_bytes = 0;

void x86_64_mm_init(u64 boot_magic, void* boot_info)
{
    mem_region_t regions[64];
    u32 region_count = 0;

    if (boot_magic == MULTIBOOT2_BOOTLOADER_MAGIC && boot_info != NULL) {
        u8* base = (u8*)boot_info;
        u32 total_size = *(u32*)base;
        u8* tag = base + 8;
        u8* end = base + total_size;

        while (tag < end) {
            mb2_tag_t* t = (mb2_tag_t*)tag;
            if (t->type == MB2_TAG_END)
                break;

            if (t->type == MB2_TAG_MMAP) {
                mb2_tag_mmap_t* mmap = (mb2_tag_mmap_t*)tag;
                u8* entry     = tag + sizeof(mb2_tag_mmap_t);
                u8* mmap_end  = tag + mmap->size;

                while (entry < mmap_end && region_count < ARRAY_SIZE(regions)) {
                    mb2_mmap_entry_t* e = (mb2_mmap_entry_t*)entry;

                    if (e->type == MB2_MEM_AVAILABLE) {
                        total_ram_bytes += e->len;   /* report true detected RAM */

                        u64 lo = e->addr;
                        u64 hi = e->addr + e->len;
                        if (lo < LOW_MEM_RESERVED)   lo = LOW_MEM_RESERVED;
                        if (hi > IDENTITY_MAP_LIMIT) hi = IDENTITY_MAP_LIMIT;

                        if (lo < hi) {
                            regions[region_count].base   = lo;
                            regions[region_count].length = hi - lo;
                            region_count++;
                        }
                    }

                    entry += mmap->entry_size;
                }
            }

            tag += ALIGN_UP(t->size, 8);
        }
    }

    pmm_init(regions, region_count, (physaddr)_kernel_start, (physaddr)_kernel_end);
}

u64 x86_64_total_ram(void)
{
    return total_ram_bytes;
}
