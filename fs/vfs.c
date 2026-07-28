/* fs/vfs.c
 * See include/fs/vfs.h. On-disk format (written by tools/mkfs.py):
 *
 *   Sector 0            : superblock { magic, file_count,
 *                          dir_start_sector, data_start_sector }
 *   [dir_start_sector..) : directory entries, 64 bytes each,
 *                          8 per sector { name[32], start_sector,
 *                          size_bytes, reserved[24] }
 *   [data_start_sector..): file contents, each padded out to a
 *                          sector boundary
 */

#include <fs/vfs.h>
#include <arch/arch.h>

#define MKFS_MAGIC       0x4D4B4653u   /* "MKFS" */
#define SECTOR_SIZE      512
#define DIR_ENTRY_SIZE   64
#define ENTRIES_PER_SECTOR (SECTOR_SIZE / DIR_ENTRY_SIZE)
#define NAME_LEN         32

typedef struct {
    u32 magic;
    u32 file_count;
    u32 dir_start_sector;
    u32 data_start_sector;
} __attribute__((packed)) superblock_t;

typedef struct {
    char name[NAME_LEN];
    u32  start_sector;
    u32  size_bytes;
    u8   reserved[DIR_ENTRY_SIZE - NAME_LEN - 8];
} __attribute__((packed)) dirent_t;

static bool         available = false;
static superblock_t sb;

static bool str_eq(const char* a, const char* b)
{
    while (*a && *b) {
        if (*a != *b)
            return false;
        a++; b++;
    }
    return *a == *b;
}

void vfs_init(void)
{
    if (!arch_disk_available())
        return;

    u8 sector[SECTOR_SIZE];
    if (!arch_disk_read_sector(0, sector))
        return;

    const superblock_t* raw = (const superblock_t*)sector;
    if (raw->magic != MKFS_MAGIC)
        return;

    sb = *raw;
    available = true;
}

bool vfs_available(void)
{
    return available;
}

bool vfs_read_file(const char* name, void* buf, u32 buf_size, u32* out_size)
{
    if (!available)
        return false;

    u32 remaining  = sb.file_count;
    u32 dir_sector = sb.dir_start_sector;

    while (remaining > 0) {
        u8 sector[SECTOR_SIZE];
        if (!arch_disk_read_sector(dir_sector, sector))
            return false;

        u32 n = remaining < ENTRIES_PER_SECTOR ? remaining : ENTRIES_PER_SECTOR;
        for (u32 i = 0; i < n; i++) {
            const dirent_t* e = (const dirent_t*)(sector + i * DIR_ENTRY_SIZE);
            if (!str_eq(e->name, name))
                continue;

            u32 size = e->size_bytes;
            u32 to_copy = size < buf_size ? size : buf_size;
            u32 sectors_needed = (size + SECTOR_SIZE - 1) / SECTOR_SIZE;
            u8* dst = (u8*)buf;
            u32 copied = 0;

            for (u32 s = 0; s < sectors_needed && copied < to_copy; s++) {
                u8 tmp[SECTOR_SIZE];
                if (!arch_disk_read_sector(e->start_sector + s, tmp))
                    return false;

                u32 chunk = to_copy - copied;
                if (chunk > SECTOR_SIZE)
                    chunk = SECTOR_SIZE;
                for (u32 b = 0; b < chunk; b++)
                    dst[copied + b] = tmp[b];
                copied += chunk;
            }

            if (out_size)
                *out_size = size;
            return true;
        }

        remaining  -= n;
        dir_sector += 1;
    }

    return false;
}
