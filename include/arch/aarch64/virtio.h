/* include/arch/aarch64/virtio.h
 * Generic virtio-mmio transport + a single split virtqueue per
 * device. QEMU's `virt` machine exposes 32 virtio-mmio slots at
 * 0x0a000000 + n*0x200 (confirmed via `-machine virt,dumpdtb`); each
 * slot is probed by MagicValue/DeviceID rather than assuming a fixed
 * slot-to-device mapping, since that depends on `-device` ordering
 * on the QEMU command line.
 *
 * Two usage patterns are supported:
 *   - Synchronous request/response (virtio_submit_and_wait) — submit
 *     a descriptor chain, kick, poll the used ring until it
 *     completes. What virtio-blk uses; one outstanding request at a
 *     time, no virtio interrupt handling needed.
 *   - Async event buffers (virtio_poll_used/virtio_publish_avail) —
 *     pre-post a pool of device-writable buffers, then poll for
 *     whichever ones the device has filled in, recycling each right
 *     away. What virtio-input uses for keyboard/mouse events.
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

/* Finds+initializes the first device matching device_id (VIRTIO_DEV_*):
 * negotiates VIRTIO_F_VERSION_1 only, sets up virtqueue 0, sets
 * DRIVER_OK, and returns true with *dev filled in. False if no
 * matching device exists or setup failed. */
bool virtio_mmio_probe(u32 device_id, virtio_device_t* dev);

/* Like virtio_mmio_probe, but finds the (index)-th device (0-based)
 * matching device_id — for device types QEMU can attach more than
 * one of (e.g. two VIRTIO_DEV_INPUT devices: keyboard + mouse). */
bool virtio_mmio_probe_nth(u32 device_id, u32 index, virtio_device_t* dev);

/* Device-specific config space (offset 0x100 + off) — used to tell
 * same-device-ID devices apart, e.g. reading VIRTIO_INPUT_CFG_ID_NAME
 * to tell a virtio-input keyboard from a mouse. */
u8   virtio_config_read8(virtio_device_t* dev, u32 off);
void virtio_config_write8(virtio_device_t* dev, u32 off, u8 val);

/* Builds a chain from `descs`/`count` (descs[i].next is overwritten
 * to link the chain — caller only sets addr/len/flags), submits it,
 * kicks the queue, and polls the used ring until this exact request
 * completes. Returns the device-reported used length. */
u32 virtio_submit_and_wait(virtio_device_t* dev, virtq_desc_t* descs, u16 count);

/* Non-blocking: if the device has completed a previously-published
 * buffer, fills desc_index/len and returns true; otherwise returns
 * false immediately without waiting. */
bool virtio_poll_used(virtio_device_t* dev, u16* desc_index, u32* len);

/* Publishes descriptor `desc_index` (already populated by the caller,
 * e.g. via dev->desc[i] = ...) as available for the device. */
void virtio_publish_avail(virtio_device_t* dev, u16 desc_index);

#endif /* ARCH_AARCH64_VIRTIO_H */
