/* include/fs/vfs.h
 * Minimal read-only filesystem layer. Arch-independent — built
 * entirely on arch_disk_available()/arch_disk_read_sector().
 *
 * Reads the tiny on-disk format tools/mkfs.py writes: a superblock
 * (sector 0), a flat directory table of fixed-size entries, and file
 * data starting at a sector boundary. No subdirectories, no writes —
 * enough to prove disk -> driver -> filesystem works end to end.
 */

#ifndef FS_VFS_H
#define FS_VFS_H

#include <kernel/types.h>

void vfs_init(void);
bool vfs_available(void);

/* Reads up to buf_size bytes of `name` into buf. On success returns
 * true and sets *out_size to the file's real size (may be larger
 * than what fit in buf_size). */
bool vfs_read_file(const char* name, void* buf, u32 buf_size, u32* out_size);

#endif /* FS_VFS_H */
