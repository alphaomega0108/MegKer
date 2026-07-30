/* arch/aarch64/drivers/virtio.c
 * See include/arch/aarch64/virtio.h for scope. virtio-mmio "modern"
 * (version 2) transport only — this kernel's QEMU target doesn't
 * need legacy support, so unlike x86_64's ATA driver there's no
 * fallback path to keep simple. QEMU's virtio-mmio devices default
 * to the legacy interface, so the Makefile passes
 * `-global virtio-mmio.force-legacy=false`.
 *
 * Descriptor/avail/used rings each get their own PMM frame — one
 * page apiece is wasteful for an 8-entry queue, but simple, and PMM
 * frames are the only allocation granularity available this early.
 * Those frames are within the boot-identity-mapped RAM range, so
 * treating their physical address as a pointer (and as the address
 * QEMU's DMA-coherent virtio device reads/writes) is valid the same
 * way it already is for every other physical-frame user in this
 * kernel (heap, page tables, ELF segments).
 */

#include <arch/aarch64/virtio.h>
#include <mm/pmm.h>

#define VIRTIO_MMIO_BASE0   0x0a000000UL
#define VIRTIO_MMIO_STRIDE  0x200UL
#define VIRTIO_MMIO_SLOTS   32

#define VIRTIO_MMIO_MAGIC_VALUE          0x000
#define VIRTIO_MMIO_VERSION              0x004
#define VIRTIO_MMIO_DEVICE_ID            0x008
#define VIRTIO_MMIO_DRIVER_FEATURES      0x020
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL  0x024
#define VIRTIO_MMIO_QUEUE_SEL            0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX        0x034
#define VIRTIO_MMIO_QUEUE_NUM            0x038
#define VIRTIO_MMIO_QUEUE_READY          0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY         0x050
#define VIRTIO_MMIO_STATUS               0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW       0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH      0x084
#define VIRTIO_MMIO_QUEUE_AVAIL_LOW      0x090
#define VIRTIO_MMIO_QUEUE_AVAIL_HIGH     0x094
#define VIRTIO_MMIO_QUEUE_USED_LOW       0x0a0
#define VIRTIO_MMIO_QUEUE_USED_HIGH      0x0a4

#define VIRTIO_MAGIC 0x74726976u   /* "virt" */

#define VIRTIO_STATUS_ACKNOWLEDGE 1u
#define VIRTIO_STATUS_DRIVER      2u
#define VIRTIO_STATUS_DRIVER_OK   4u
#define VIRTIO_STATUS_FEATURES_OK 8u

#define QUEUE_SIZE 8

typedef struct { u32 id; u32 len; } used_elem_t;

static inline u32 mmio_read(volatile u8* base, u32 off)
{
    return *(volatile u32*)(base + off);
}

static inline void mmio_write(volatile u8* base, u32 off, u32 val)
{
    *(volatile u32*)(base + off) = val;
}

static u16* avail_idx(virtio_device_t* d)   { return (u16*)(d->avail + 2); }
static u16* avail_ring(virtio_device_t* d)  { return (u16*)(d->avail + 4); }
static u16* used_idx(virtio_device_t* d)    { return (u16*)(d->used + 2); }
static used_elem_t* used_ring(virtio_device_t* d) { return (used_elem_t*)(d->used + 4); }

bool virtio_mmio_probe(u32 device_id, virtio_device_t* dev)
{
    volatile u8* base = NULL;

    for (u32 i = 0; i < VIRTIO_MMIO_SLOTS; i++) {
        volatile u8* candidate = (volatile u8*)(VIRTIO_MMIO_BASE0 + i * VIRTIO_MMIO_STRIDE);
        if (mmio_read(candidate, VIRTIO_MMIO_MAGIC_VALUE) != VIRTIO_MAGIC)
            continue;
        if (mmio_read(candidate, VIRTIO_MMIO_DEVICE_ID) != device_id)
            continue;
        base = candidate;
        break;
    }
    if (!base)
        return false;

    if (mmio_read(base, VIRTIO_MMIO_VERSION) != 2)
        return false;   /* legacy-only device — not supported */

    mmio_write(base, VIRTIO_MMIO_STATUS, 0);   /* reset */
    mmio_write(base, VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE);
    mmio_write(base, VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);

    /* Negotiate nothing but VIRTIO_F_VERSION_1 (bit 32, i.e. bit 0 of
     * the feature-select-1 word) — no optional features used. */
    mmio_write(base, VIRTIO_MMIO_DRIVER_FEATURES_SEL, 0);
    mmio_write(base, VIRTIO_MMIO_DRIVER_FEATURES, 0);
    mmio_write(base, VIRTIO_MMIO_DRIVER_FEATURES_SEL, 1);
    mmio_write(base, VIRTIO_MMIO_DRIVER_FEATURES, 1);

    mmio_write(base, VIRTIO_MMIO_STATUS,
               VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK);
    if (!(mmio_read(base, VIRTIO_MMIO_STATUS) & VIRTIO_STATUS_FEATURES_OK))
        return false;

    mmio_write(base, VIRTIO_MMIO_QUEUE_SEL, 0);
    if (mmio_read(base, VIRTIO_MMIO_QUEUE_NUM_MAX) < QUEUE_SIZE)
        return false;
    mmio_write(base, VIRTIO_MMIO_QUEUE_NUM, QUEUE_SIZE);

    physaddr desc_frame  = pmm_alloc_frame();
    physaddr avail_frame = pmm_alloc_frame();
    physaddr used_frame  = pmm_alloc_frame();
    if (!desc_frame || !avail_frame || !used_frame)
        return false;

    u8* desc_p  = (u8*)desc_frame;
    u8* avail_p = (u8*)avail_frame;
    u8* used_p  = (u8*)used_frame;
    for (int i = 0; i < 4096; i++) { desc_p[i] = 0; avail_p[i] = 0; used_p[i] = 0; }

    mmio_write(base, VIRTIO_MMIO_QUEUE_DESC_LOW,  (u32)desc_frame);
    mmio_write(base, VIRTIO_MMIO_QUEUE_DESC_HIGH, (u32)(desc_frame >> 32));
    mmio_write(base, VIRTIO_MMIO_QUEUE_AVAIL_LOW,  (u32)avail_frame);
    mmio_write(base, VIRTIO_MMIO_QUEUE_AVAIL_HIGH, (u32)(avail_frame >> 32));
    mmio_write(base, VIRTIO_MMIO_QUEUE_USED_LOW,  (u32)used_frame);
    mmio_write(base, VIRTIO_MMIO_QUEUE_USED_HIGH, (u32)(used_frame >> 32));
    mmio_write(base, VIRTIO_MMIO_QUEUE_READY, 1);

    mmio_write(base, VIRTIO_MMIO_STATUS,
               VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER |
               VIRTIO_STATUS_FEATURES_OK | VIRTIO_STATUS_DRIVER_OK);

    dev->mmio_base  = base;
    dev->queue_size = QUEUE_SIZE;
    dev->desc       = (virtq_desc_t*)desc_p;
    dev->avail      = avail_p;
    dev->used       = used_p;
    dev->next_desc  = 0;
    dev->used_seen  = 0;
    return true;
}

u32 virtio_submit_and_wait(virtio_device_t* dev, virtq_desc_t* descs, u16 count)
{
    for (u16 i = 0; i < count; i++) {
        dev->desc[i].addr  = descs[i].addr;
        dev->desc[i].len   = descs[i].len;
        dev->desc[i].flags = descs[i].flags | (u16)((i + 1 < count) ? VIRTQ_DESC_F_NEXT : 0);
        dev->desc[i].next  = (u16)((i + 1 < count) ? (i + 1) : 0);
    }

    u16 slot = (u16)(*avail_idx(dev) % dev->queue_size);
    avail_ring(dev)[slot] = 0;   /* head descriptor index — always 0, one request at a time */
    __asm__ volatile ("dmb ish" ::: "memory");
    (*avail_idx(dev))++;
    __asm__ volatile ("dmb ish" ::: "memory");

    mmio_write(dev->mmio_base, VIRTIO_MMIO_QUEUE_NOTIFY, 0);

    while (*used_idx(dev) == dev->used_seen) { }
    __asm__ volatile ("dmb ish" ::: "memory");

    u32 len = used_ring(dev)[dev->used_seen % dev->queue_size].len;
    dev->used_seen++;
    return len;
}
