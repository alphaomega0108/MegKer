ENTRY(_start)

SECTIONS
{
    . = 0x100000;

    /* Multiboot header must be very first thing */
    .multiboot ALIGN(8) :
    {
        *(.multiboot)
    }

    /* Code — _start must be first */
    .text ALIGN(4096) :
    {
        *(.text._start)
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
        /* Boot page tables + boot stack: already initialized by boot.asm
         * (page tables are live under CR3 by the time long_mode_entry
         * runs) — must stay outside the C-bss zero range below. */
        *(.bss.boot)

        _bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        _bss_end = .;
    }

    _kernel_end = .;

    /* Discard debug info */
    /DISCARD/ :
    {
        *(.eh_frame)
        *(.comment)
    }
}