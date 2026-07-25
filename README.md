# MegKer

A kernel aimed at running across as wide a span of processors as possible —
from 8-bit microcontrollers like the **8051**, up through **8086**,
**Pentium**, **Core 2 Duo**, and modern 64-bit silicon like **Apple's M4**.

That's an intentionally extreme range. An 8051 has no MMU and a few hundred
bytes of RAM; an M4 has 64-bit virtual memory and multiple cores. MegKer
handles this by keeping a strict split between arch-independent kernel code
and per-architecture backends, so each target only implements what it's
actually capable of — the core kernel never assumes an MMU, multi-core, or
even a particular word size exists.

## Current status

Only **x86_64** is implemented so far. It boots under GRUB2 (Multiboot2), can
print to a VGA text console, and handles CPU exceptions and hardware
interrupts through a working IDT/PIC/PIT setup.

`aarch64`, `arm32`, and MCU targets (`rp2040`, `stm32f4`) exist as empty
scaffolding in `arch/` and in the Makefile, but have no code yet.

### x86_64 boot sequence

1. GRUB2 loads the Multiboot2 kernel at `0x100000` and jumps to `_start` in
   32-bit protected mode (`arch/x86_64/boot/boot.asm`).
2. `_start` sets up a boot stack, checks for Long Mode support via CPUID,
   builds identity-mapped page tables (2MB pages, first 1GB), enables PAE,
   loads a minimal 64-bit GDT, and far-jumps into Long Mode.
3. `long_mode_entry` reloads segment registers, zeroes the C `.bss` region,
   and calls `kernel_main()`.
4. `kernel_main()` (`kernel/kernel.c`) — the arch-independent entry point —
   brings the system up in order: console, memory, interrupts, timer, then
   drops into an idle loop.

### What's implemented

| Subsystem  | File(s)                                                   | Status |
|------------|------------------------------------------------------------|--------|
| Boot       | `arch/x86_64/boot/boot.asm`, `linker.ld`                   | Long Mode entry, identity-mapped first 1GB |
| GDT        | `arch/x86_64/boot/gdt.c`, `gdt_flush.asm`                  | Minimal flat GDT (kernel code/data) |
| IDT        | `arch/x86_64/boot/idt.c`, `isr_stubs.asm`                  | All 256 vectors; 32 CPU exceptions + 16 IRQs wired |
| PIC        | `arch/x86_64/boot/idt.c` (`pic_remap`)                     | 8259 remapped so IRQs land at vectors 32-47 |
| Timer      | `arch/x86_64/drivers/pit.c`                                | PIT (8253/8254) driving IRQ0 at a configurable Hz |
| Console    | `arch/x86_64/drivers/console.c`                             | VGA text mode (0xB8000), scrolling |
| Memory     | `arch/x86_64/arch.c` (`arch_mm_init`)                       | Stub — not implemented yet |

Unhandled CPU exceptions (divide-by-zero, GPF, page fault, etc.) currently
call `kernel_panic()` with the exception name and halt, rather than
crashing silently.

## Architecture abstraction

The core kernel (`kernel/kernel.c`) never touches hardware directly — it
only calls the functions declared in `include/arch/arch.h`:

```c
arch_name();                          // "x86_64", "aarch64", ...
arch_early_init();  arch_late_init(); // boot-time hooks
arch_console_init/putc/clear();       // console output
arch_interrupts_init/enable/disable();// IDT/GIC/etc
arch_mm_init();  arch_get_total_ram();// paging/MMU
arch_cpu_halt();  arch_cpu_relax();   // CPU control
arch_timer_init(hz);  arch_timer_ticks();
```

Every architecture under `arch/<name>/` implements all of these. Fixed-width
types (`include/kernel/types.h`) are used everywhere instead of `int`/`long`,
since their size isn't consistent across the target range.

## Project layout

```
arch/                Per-architecture code (boot, GDT/IDT, drivers, mm)
  x86_64/             Implemented
  aarch64/, arm32/    Scaffolding only
  mcu/rp2040/         )
  mcu/stm32f4/        )  Scaffolding only
include/              Public headers (kernel/, arch/, drivers/, lib/)
kernel/               Arch-independent kernel core (kernel_main, panic)
drivers/              Arch-independent drivers (not yet populated)
mm/                   Arch-independent memory management (not yet populated)
lib/                  Freestanding libc-ish helpers (not yet populated)
gui/                  Future GUI work (not yet populated)
tools/check-toolchain.sh   Checks that the expected cross-toolchains/QEMU are installed
```

## Building & running

Requires, per target architecture:

- A `<target>-elf-gcc` cross-compiler (e.g. `x86_64-elf-gcc`)
- `nasm` (x86_64 only)
- `qemu-system-<arch>`
- For bootable ISOs: a GRUB build that includes the **`i386-pc`** platform
  files (`grub-mkrescue`). Note: Homebrew's `x86_64-elf-grub` only ships the
  `x86_64-efi` platform and cannot produce a BIOS-bootable ISO — use
  `i686-elf-grub` instead (`brew install i686-elf-grub`).

Run `tools/check-toolchain.sh` to verify what's installed.

```sh
make                 # build x86_64 kernel.elf (default ARCH=x86_64)
make iso             # package it into a bootable ISO with GRUB2
make run             # build + launch in QEMU
make clean           # remove build/
make all-archs       # build every arch (aarch64/arm32 will no-op until implemented)
```

`make run` boots the ISO in QEMU (`-machine q35`) with serial output to
stdio. You should see GRUB's menu, then a cleared screen with the `MegKer`
banner.

## Known gaps / next steps

- `arch_mm_init()` is a stub — no physical/virtual memory manager yet.
- No keyboard or other IRQ-driven drivers beyond the timer.
- `aarch64`, `arm32`, and the MCU targets have no code — everything above is
  x86_64-only so far.
- No userspace, no scheduler, no filesystem yet.
