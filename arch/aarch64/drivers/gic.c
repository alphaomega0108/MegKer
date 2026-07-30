/* arch/aarch64/drivers/gic.c
 * GICv2, Distributor + CPU interface only (no redistributors — that's
 * GICv3). QEMU's `virt` machine maps these at fixed addresses
 * regardless of which CPU model is emulated.
 *
 * We boot non-secure (SCR_EL3.NS=1 in boot.S), so interrupts have to
 * be assigned to Group 1 to reach EL1 as IRQs at all — left in
 * Group 0, QEMU's GIC model either routes them to a secure world we
 * don't have, or as FIQ, neither of which this kernel handles yet.
 */

#include <arch/aarch64/gic.h>

#define GICD_BASE 0x08000000UL
#define GICC_BASE 0x08010000UL

#define GICD_CTLR         (*(volatile u32*)(GICD_BASE + 0x000))
#define GICD_IGROUPR(n)   (*(volatile u32*)(GICD_BASE + 0x080 + 4 * (n)))
#define GICD_IPRIORITYR(n) (*(volatile u32*)(GICD_BASE + 0x400 + 4 * (n)))
#define GICD_ISENABLER(n) (*(volatile u32*)(GICD_BASE + 0x100 + 4 * (n)))

#define GICC_CTLR (*(volatile u32*)(GICC_BASE + 0x000))
#define GICC_PMR  (*(volatile u32*)(GICC_BASE + 0x004))
#define GICC_IAR  (*(volatile u32*)(GICC_BASE + 0x00C))
#define GICC_EOIR (*(volatile u32*)(GICC_BASE + 0x010))

#define GICD_CTLR_ENABLE_GRP0 (1u << 0)
#define GICD_CTLR_ENABLE_GRP1 (1u << 1)
#define GICC_CTLR_ENABLE_GRP0 (1u << 0)
#define GICC_CTLR_ENABLE_GRP1 (1u << 1)
#define GICC_CTLR_ACK_CTL     (1u << 2)

void gic_init(void)
{
    GICD_CTLR = GICD_CTLR_ENABLE_GRP0 | GICD_CTLR_ENABLE_GRP1;

    /* AckCtl: our CPU access reads as Secure here (no EL3 firmware
     * separately owns the secure world), so without this bit a
     * pending Group 1 interrupt only ever shows up as GICC_IAR=1022
     * (the "there's a Group1 IRQ, but you're Secure" proxy ID)
     * instead of its real INTID. */
    GICC_CTLR = GICC_CTLR_ENABLE_GRP0 | GICC_CTLR_ENABLE_GRP1 | GICC_CTLR_ACK_CTL;
    GICC_PMR  = 0xFFu;   /* don't mask any priority */
}

void gic_enable_irq(u32 irq)
{
    u32 reg  = irq / 32;
    u32 bit  = irq % 32;

    GICD_IGROUPR(reg) |= (1u << bit);                              /* Group 1 — reaches EL1 as IRQ */
    GICD_IPRIORITYR(irq / 4) &= ~(0xFFu << ((irq % 4) * 8));        /* priority 0 — highest */
    GICD_ISENABLER(reg) = (1u << bit);
}

u32 gic_cpu_iack(void)
{
    return GICC_IAR;
}

void gic_cpu_eoi(u32 iar)
{
    GICC_EOIR = iar;
}
