/* include/arch/aarch64/virtio_blk.h
 * virtio-blk — read-only usage, mirrors arch/x86_64/ata.h's shape so
 * it plugs into arch_disk_available()/arch_disk_read_sector() the
 * same way.
 */

#ifndef ARCH_AARCH64_VIRTIO_BLK_H
#define ARCH_AARCH64_VIRTIO_BLK_H

#include <kernel/types.h>

void virtio_blk_init(void);
bool virtio_blk_available(void);
bool virtio_blk_read_sector(u32 lba, u8* buf);

#endif /* ARCH_AARCH64_VIRTIO_BLK_H */
