/* include/arch/aarch64/virtio.h
 * Generic virtio-mmio transport + a single split virtqueue per
 * device. QEMU's `virt` machine exposes 32 virtio-mmio slots at
 * 0x0a000000 + n*0x200 (confirmed via `-machine virt,dumpdtb`); each
 * slot is probed by MagicValue/DeviceID rather than assuming a fixed
 * slot-to-device mapping, since that depends on `-device` ordering
 * on the QEMU command line.
 *
 * Deliberately synchronous, like the x86_64 ATA PIO driver: submit a
 * descriptor chain, kick the queue, then poll the used ring until it
 * completes. No virtio interrupt handling needed as a result — one
 * outstanding request at a time is enough for this kernel's needs.
 */

#ifndef ARCH_AARCH64_VIRTIO_H
#define ARCH_AARCH64_VIRTIO_H

#include <kernel/types.h>

#define VIRTIO_DEV_BLOCK 2
#define VIRTIO_DEV_GPU   16
#define VIRTIO_DEV_INPUT 18

#define VIRTQ_DESC_F_NEXT  1u   /* descriptor chain continues via `next` */
#define VIRTQ_DESC_F_WRITE 2u   /* device writes this buffer (vs. driver-written) */

typedef struct {
    u64 addr;
    u32 len;
    u16 flags;
    u16 next;
} __attribute__((packed)) virtq_desc_t;

typedef struct {
    volatile u8* mmio_base;
    u16 queue_size;
    virtq_desc_t* desc;    /* one page */
    u8*           avail;   /* one page: {u16 flags; u16 idx; u16 ring[queue_size];} */
    u8*           used;    /* one page: {u16 flags; u16 idx; {u32 id; u32 len;} ring[queue_size];} */
    u16           next_desc;  /* next free slot in the descriptor table */
    u16           used_seen;  /* last used->idx this driver has consumed */
} virtio_device_t;

/* Scans all virtio-mmio slots for the first device matching
 * device_id (VIRTIO_DEV_*). On success, negotiates VIRTIO_F_VERSION_1
 * only, sets up virtqueue 0, sets DRIVER_OK, and returns true with
 * *dev filled in. False if no matching device exists or setup failed. */
bool virtio_mmio_probe(u32 device_id, virtio_device_t* dev);

/* Builds a chain from `descs`/`count` (descs[i].next is overwritten
 * to link the chain — caller only sets addr/len/flags), submits it,
 * kicks the queue, and polls the used ring until this exact request
 * completes. Returns the device-reported used length. */
u32 virtio_submit_and_wait(virtio_device_t* dev, virtq_desc_t* descs, u16 count);

#endif /* ARCH_AARCH64_VIRTIO_H */
