/* arch/x86_64/boot/tss.c
 * Task State Segment — in long mode this exists solely to tell the
 * CPU which stack to load (RSP0) when an interrupt/syscall brings us
 * from ring 3 back to ring 0. There's no hardware task-switching here.
 */

#include <kernel/types.h>

typedef struct {
    u32 reserved0;
    u64 rsp0;
    u64 rsp1;
    u64 rsp2;
    u64 reserved1;
    u64 ist1, ist2, ist3, ist4, ist5, ist6, ist7;
    u64 reserved2;
    u16 reserved3;
    u16 iomap_base;
} __attribute__((packed)) tss_t;

#define TSS_SELECTOR       0x28   /* GDT index 5 * 8 */
#define KERNEL_STACK_SIZE  16384

static tss_t tss;
static u8 kernel_stack[KERNEL_STACK_SIZE] __attribute__((aligned(16)));

extern void gdt_set_tss_descriptor(u64 base, u32 limit);

void tss_init(void)
{
    u8* p = (u8*)&tss;
    for (usize i = 0; i < sizeof(tss); i++)
        p[i] = 0;

    tss.rsp0 = (u64)(kernel_stack + KERNEL_STACK_SIZE);
    tss.iomap_base = sizeof(tss_t);   /* no I/O bitmap */

    gdt_set_tss_descriptor((u64)&tss, sizeof(tss_t) - 1);

    __asm__ volatile ("ltr %0" :: "r"((u16)TSS_SELECTOR));
}
