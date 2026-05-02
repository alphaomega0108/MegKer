/* arch/x86_64/linker.ld
 * Linker script for MegKer x86_64
 * Kernel is loaded at 1MB (0x100000) by GRUB
 */

ENTRY(_start)

SECTIONS
{
    . = 0x100000;           /* Load address: 1MB mark */

    /* Multiboot header must come first */
    .multiboot ALIGN(8) :
    {
        *(.multiboot)
    }

    /* Code */
    .text ALIGN(4096) :
    {
        *(.text)
        *(.text.*)
    }

    /* Read-only data */
    .rodata ALIGN(4096) :
    {
        *(.rodata)
        *(.rodata.*)
    }

    /* Initialized data */
    .data ALIGN(4096) :
    {
        *(.data)
        *(.data.*)
    }

    /* Uninitialized data — zeroed at boot */
    .bss ALIGN(4096) :
    {
        _bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        _bss_end = .;
    }

    _kernel_end = .;        /* Mark end of kernel */
}