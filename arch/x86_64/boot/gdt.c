/* arch/x86_64/boot/gdt.c
 * Global Descriptor Table for x86_64.
 * In 64-bit mode the GDT is mostly legacy,
 * but we still need it for privilege levels later.
 */

#include <kernel/types.h>

/* A GDT entry is 8 bytes */
typedef struct {
    u16 limit_low;
    u16 base_low;
    u8  base_mid;
    u8  access;
    u8  flags_limit_high;
    u8  base_high;
} __attribute__((packed)) gdt_entry_t;

/* GDT pointer loaded with lgdt */
typedef struct {
    u16 limit;
    u64 base;
} __attribute__((packed)) gdt_pointer_t;

/* Our GDT: null, kernel code, kernel data */
static gdt_entry_t gdt[3];
static gdt_pointer_t gdt_ptr;

static void gdt_set_entry(int i, u32 base, u32 limit,
                           u8 access, u8 flags)
{
    gdt[i].base_low         = (base  & 0xFFFF);
    gdt[i].base_mid         = (base  >> 16) & 0xFF;
    gdt[i].base_high        = (base  >> 24) & 0xFF;
    gdt[i].limit_low        = (limit & 0xFFFF);
    gdt[i].flags_limit_high = ((limit >> 16) & 0x0F) | (flags & 0xF0);
    gdt[i].access           = access;
}

void gdt_init(void)
{
    /* 0: Null descriptor — required */
    gdt_set_entry(0, 0, 0, 0, 0);

    /* 1: Kernel Code — executable, readable, ring 0 */
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0xA0);

    /* 2: Kernel Data — writable, ring 0 */
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0xC0);

    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base  = (u64)&gdt;

    /* Load it — defined in gdt_flush.asm */
    extern void gdt_flush(u64);
    gdt_flush((u64)&gdt_ptr);
}