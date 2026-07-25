/* arch/x86_64/boot/idt.c
 * Interrupt Descriptor Table setup for x86_64.
 * Remaps the 8259 PICs so IRQs land at vectors 32-47 (0-31 are
 * reserved for CPU exceptions), installs a stub for every vector,
 * and dispatches exceptions/IRQs to C.
 */

#include <kernel/types.h>
#include <kernel/kernel.h>
#include <arch/x86_64/idt.h>
#include <arch/x86_64/io.h>

typedef struct {
    u16 offset_low;
    u16 selector;
    u8  ist;
    u8  type_attr;
    u16 offset_mid;
    u32 offset_high;
    u32 zero;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    u16 limit;
    u64 base;
} __attribute__((packed)) idt_pointer_t;

static idt_entry_t   idt[IDT_ENTRIES];
static idt_pointer_t idt_ptr;
static irq_handler_t irq_handlers[IRQ_COUNT];

/* Defined in isr_stubs.asm — one label per vector we handle. */
extern void isr0(void),  isr1(void),  isr2(void),  isr3(void);
extern void isr4(void),  isr5(void),  isr6(void),  isr7(void);
extern void isr8(void),  isr9(void),  isr10(void), isr11(void);
extern void isr12(void), isr13(void), isr14(void), isr15(void);
extern void isr16(void), isr17(void), isr18(void), isr19(void);
extern void isr20(void), isr21(void), isr22(void), isr23(void);
extern void isr24(void), isr25(void), isr26(void), isr27(void);
extern void isr28(void), isr29(void), isr30(void), isr31(void);

extern void irq0(void),  irq1(void),  irq2(void),  irq3(void);
extern void irq4(void),  irq5(void),  irq6(void),  irq7(void);
extern void irq8(void),  irq9(void),  irq10(void), irq11(void);
extern void irq12(void), irq13(void), irq14(void), irq15(void);

static const char* exception_names[32] = {
    "Divide-by-zero", "Debug", "NMI", "Breakpoint",
    "Overflow", "Bound Range Exceeded", "Invalid Opcode", "Device Not Available",
    "Double Fault", "Coprocessor Segment Overrun", "Invalid TSS", "Segment Not Present",
    "Stack-Segment Fault", "General Protection Fault", "Page Fault", "Reserved",
    "x87 Floating-Point Exception", "Alignment Check", "Machine Check", "SIMD Floating-Point Exception",
    "Virtualization Exception", "Control Protection Exception", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Hypervisor Injection Exception", "VMM Communication Exception", "Security Exception", "Reserved"
};

static void idt_set_gate(int vec, void (*handler)(void), u8 type_attr)
{
    u64 base = (u64)handler;

    idt[vec].offset_low  = base & 0xFFFF;
    idt[vec].offset_mid  = (base >> 16) & 0xFFFF;
    idt[vec].offset_high = (base >> 32) & 0xFFFFFFFF;
    idt[vec].selector    = 0x08;   /* kernel code segment, set up in gdt.c */
    idt[vec].ist         = 0;
    idt[vec].type_attr   = type_attr;
    idt[vec].zero        = 0;
}

/* 0x8E = present, ring 0, 64-bit interrupt gate */
#define GATE_INTERRUPT 0x8E

static void pic_remap(void)
{
    u8 mask1 = inb(0x21);
    u8 mask2 = inb(0xA1);

    outb(0x20, 0x11); io_wait();   /* start init sequence, cascade mode */
    outb(0xA0, 0x11); io_wait();
    outb(0x21, IRQ_BASE);       io_wait();   /* master PIC vector offset */
    outb(0xA1, IRQ_BASE + 8);   io_wait();   /* slave PIC vector offset  */
    outb(0x21, 0x04); io_wait();   /* tell master about slave on IRQ2 */
    outb(0xA1, 0x02); io_wait();   /* tell slave its cascade identity */
    outb(0x21, 0x01); io_wait();   /* 8086 mode */
    outb(0xA1, 0x01); io_wait();

    outb(0x21, mask1);   /* restore whatever was masked before */
    outb(0xA1, mask2);
}

void irq_set_mask(int irq, bool masked)
{
    u16 port = (irq < 8) ? 0x21 : 0xA1;
    u8  bit  = irq < 8 ? irq : irq - 8;
    u8  val  = inb(port);

    if (masked)
        val |= (1 << bit);
    else
        val &= ~(1 << bit);

    outb(port, val);
}

void irq_install_handler(int irq, irq_handler_t handler)
{
    irq_handlers[irq] = handler;
    irq_set_mask(irq, false);
}

void idt_init(void)
{
    void (*isr_stubs[32])(void) = {
        isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7,
        isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15,
        isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
        isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31,
    };
    void (*irq_stubs[16])(void) = {
        irq0,  irq1,  irq2,  irq3,  irq4,  irq5,  irq6,  irq7,
        irq8,  irq9,  irq10, irq11, irq12, irq13, irq14, irq15,
    };

    for (int i = 0; i < 32; i++)
        idt_set_gate(i, isr_stubs[i], GATE_INTERRUPT);

    pic_remap();

    /* Mask everything until a driver explicitly wants an IRQ. */
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);

    for (int i = 0; i < 16; i++)
        idt_set_gate(IRQ_BASE + i, irq_stubs[i], GATE_INTERRUPT);

    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base  = (u64)&idt;
    __asm__ volatile ("lidt %0" :: "m"(idt_ptr));
}

/* Called from isr_common_stub for every vector 0-47. */
void isr_handler(registers_t* regs)
{
    if (regs->int_no < 32) {
        kernel_panic(exception_names[regs->int_no]);
        return; /* unreachable — kernel_panic halts */
    }

    int irq = (int)(regs->int_no - IRQ_BASE);

    if (irq_handlers[irq])
        irq_handlers[irq](regs);

    if (irq >= 8)
        outb(0xA0, 0x20);   /* EOI to slave PIC */
    outb(0x20, 0x20);       /* EOI to master PIC */
}
