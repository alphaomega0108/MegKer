/* arch/aarch64/drivers/virtio_blk.c
 * Read-only virtio-blk on top of the generic virtio-mmio transport
 * (virtio.c). Each read is a 3-descriptor chain: a driver-written
 * request header, a device-written 512-byte data buffer, and a
 * device-written 1-byte status — the standard virtio-blk protocol.
 *
 * Buffers passed here (the header/status this file owns, and the
 * caller's `buf`) all have to be physical addresses the device can
 * DMA into/out of directly; that's true for any kernel-space pointer
 * without extra work because it's within the boot VMM's identity-
 * mapped RAM range, same as every other physical-frame user in this
 * kernel (heap, page tables, ELF segments).
 */

#include <arch/aarch64/virtio_blk.h>
#include <arch/aarch64/virtio.h>
#include <mm/heap.h>

#define VIRTIO_BLK_T_IN 0   /* read */

typedef struct {
    u32 type;
    u32 reserved;
    u64 sector;
} __attribute__((packed)) virtio_blk_req_header_t;

static virtio_device_t dev;
static bool disk_present = false;
static virtio_blk_req_header_t* req_hdr;
static u8* req_status;

void virtio_blk_init(void)
{
    disk_present = virtio_mmio_probe(VIRTIO_DEV_BLOCK, &dev);
    if (!disk_present)
        return;

    req_hdr    = (virtio_blk_req_header_t*)kmalloc(sizeof(virtio_blk_req_header_t));
    req_status = (u8*)kmalloc(1);
}

bool virtio_blk_available(void)
{
    return disk_present;
}

bool virtio_blk_read_sector(u32 lba, u8* buf)
{
    if (!disk_present)
        return false;

    req_hdr->type     = VIRTIO_BLK_T_IN;
    req_hdr->reserved = 0;
    req_hdr->sector   = lba;

    virtq_desc_t descs[3] = {
        { .addr = (u64)(usize)req_hdr, .len = sizeof(*req_hdr), .flags = 0 },
        { .addr = (u64)(usize)buf,     .len = 512,              .flags = VIRTQ_DESC_F_WRITE },
        { .addr = (u64)(usize)req_status, .len = 1,             .flags = VIRTQ_DESC_F_WRITE },
    };
    virtio_submit_and_wait(&dev, descs, 3);

    return *req_status == 0;
}
