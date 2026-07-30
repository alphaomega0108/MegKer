ENTRY(_start)

SECTIONS
{
    /* QEMU's `virt` machine has RAM starting at 0x40000000; loaded far
     * enough in to leave room below for anything QEMU itself expects
     * there. Since we ship a real ELF (not a raw Image), QEMU's loader
     * places segments at these linked addresses directly. */
    . = 0x40080000;
    _kernel_start = .;

    .text ALIGN(4096) :
    {
        *(.text.boot)
        *(.text)
        *(.text.*)
    }

    .rodata ALIGN(4096) :
    {
        *(.rodata)
        *(.rodata.*)
    }

    .data ALIGN(4096) :
    {
        *(.data)
        *(.data.*)
    }

    .bss ALIGN(4096) :
    {
        /* Boot stack: already in use by the time boot.S's bss-clearing
         * runs — must stay outside the C-bss zero range below. */
        *(.bss.boot)

        _bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        _bss_end = .;
    }

    _kernel_end = .;

    /DISCARD/ :
    {
        *(.eh_frame)
        *(.comment)
        *(.note.*)
    }
}
