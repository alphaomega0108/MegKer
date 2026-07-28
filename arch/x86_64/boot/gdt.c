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

/* Our GDT: null, kernel code, kernel data, user code, user data,
 * TSS (16 bytes — spans two slots in long mode). */
static gdt_entry_t gdt[7];
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

/* Called by tss_init() once the TSS itself exists — fills in the
 * 16-byte system descriptor (slots 5+6) that a normal 8-byte
 * gdt_set_entry() can't represent. */
void gdt_set_tss_descriptor(u64 base, u32 limit)
{
    gdt_set_entry(5, (u32)base, limit, 0x89, 0x00);   /* present, DPL0, 64-bit TSS (available) */

    u32* upper = (u32*)&gdt[6];
    upper[0] = (u32)(base >> 32);
    upper[1] = 0;
}

void gdt_init(void)
{
    /* 0: Null descriptor — required */
    gdt_set_entry(0, 0, 0, 0, 0);

    /* 1: Kernel Code — executable, readable, ring 0 */
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0xA0);

    /* 2: Kernel Data — writable, ring 0 */
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0xC0);

    /* 3: User Code — executable, readable, ring 3 */
    gdt_set_entry(3, 0, 0xFFFFF, 0xFA, 0xA0);

    /* 4: User Data — writable, ring 3 */
    gdt_set_entry(4, 0, 0xFFFFF, 0xF2, 0xC0);

    /* 5+6 (TSS): left zeroed until tss_init() calls gdt_set_tss_descriptor() */

    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base  = (u64)&gdt;

    /* Load it — defined in gdt_flush.asm */
    extern void gdt_flush(u64);
    gdt_flush((u64)&gdt_ptr);
}