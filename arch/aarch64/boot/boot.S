/* arch/aarch64/boot/boot.S
 * AArch64 entry point for QEMU's `virt` machine. QEMU's ELF loader
 * places us at our linked address and jumps to _start directly — no
 * bootloader in between. x0 holds a device-tree blob pointer per the
 * standard aarch64 boot convention; we forward it to kernel_main() as
 * boot_info (aarch64's equivalent of x86_64's Multiboot2 info ptr).
 *
 * QEMU can reset the CPU into EL1, EL2, or EL3 depending on machine
 * config, so we check CurrentEL and drop down to EL1 before running
 * any kernel code, since EL1 is the only level this kernel targets.
 */

.section ".text.boot"
.global _start

_start:
    mov x19, x0                 /* stash the DTB pointer before x0 gets clobbered */

    mrs x1, mpidr_el1
    and x1, x1, #0xff
    cbnz x1, park                /* only CPU 0 continues; secondaries wait */

    mrs x1, CurrentEL
    lsr x1, x1, #2
    cmp x1, #3
    b.eq from_el3
    cmp x1, #2
    b.eq from_el2
    b el1_entry

from_el3:
    mov x1, #0x5b1               /* SCR_EL3: RW|HCE|SMD|NS + RES1 bits */
    msr scr_el3, x1
    mov x1, #0x3c9                /* SPSR: EL2h, all exceptions masked */
    msr spsr_el3, x1
    adr x1, from_el2
    msr elr_el3, x1
    eret

from_el2:
    mov x1, #(1 << 31)           /* HCR_EL2.RW — EL1 runs in AArch64 */
    msr hcr_el2, x1

    mov x1, #3                   /* CNTHCTL_EL2: EL1PCTEN|EL1PCEN — don't
                                   * trap EL1's access to the physical
                                   * timer/counter to a nonexistent EL2 */
    msr cnthctl_el2, x1
    msr cntvoff_el2, xzr

    mov x1, #0x3c5                /* SPSR: EL1h, all exceptions masked */
    msr spsr_el2, x1
    adr x1, el1_entry
    msr elr_el2, x1
    eret

el1_entry:
    ldr x1, =boot_stack_top
    mov sp, x1

    ldr x1, =_bss_start
    ldr x2, =_bss_end
clear_bss:
    cmp x1, x2
    b.ge bss_cleared
    str xzr, [x1], #8
    b clear_bss
bss_cleared:
    mov x0, #0                   /* boot_magic — unused on aarch64 */
    mov x1, x19                  /* boot_info — the DTB pointer */
    bl kernel_main

park:
    wfe
    b park

/* Boot stack: already in use by the time bss-clearing runs above, so
 * it's kept in its own section outside the _bss_start.._bss_end zero
 * range (mirrors the x86_64 boot.asm convention for the same reason). */
.section ".bss.boot", "aw", %nobits
.align 4
boot_stack_bottom:
    .space 0x4000
boot_stack_top:
