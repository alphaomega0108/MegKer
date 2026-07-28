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

**x86_64 is a complete, if minimal, monolithic OS**: it boots via GRUB2/
Multiboot2, drives a graphical GUI with mouse and keyboard, preemptively
schedules kernel threads, runs real ELF64 executables in ring 3 with
syscalls and isolated address spaces, and reads files off a disk through a
small VFS. All of it runs together, verified working concurrently.

`aarch64`, `arm32`, and MCU targets (`rp2040`, `stm32f4`) exist as empty
scaffolding in `arch/` and in the Makefile, but have no code yet — the
entire span from 8051 to M4 that this project is named for is still
represented by exactly one architecture so far.

### x86_64 boot sequence

1. GRUB2 loads the Multiboot2 kernel at `0x100000` and jumps to `_start` in
   32-bit protected mode (`arch/x86_64/boot/boot.asm`).
2. `_start` sets up a boot stack, checks for Long Mode support via CPUID,
   builds identity-mapped page tables (2MB pages, first 1GB), enables PAE,
   loads a minimal 64-bit GDT, and far-jumps into Long Mode.
3. `long_mode_entry` reloads segment registers, recovers the Multiboot2
   magic/info pointer GRUB left in `eax`/`ebx`, zeroes the C `.bss` region,
   and calls `kernel_main(magic, info)`.
4. `kernel_main()` (`kernel/kernel.c`) — the arch-independent entry point —
   brings up console, memory (PMM + VMM), interrupts, the timer, graphics,
   input, the filesystem, and the scheduler, then the boot thread joins the
   scheduler's idle/GUI loop.

### What's implemented

| Subsystem     | File(s)                                                        | Notes |
|---------------|-----------------------------------------------------------------|-------|
| Boot          | `arch/x86_64/boot/boot.asm`, `linker.ld`                        | Long Mode entry, identity-mapped first 1GB |
| GDT + TSS     | `arch/x86_64/boot/gdt.c`, `gdt_flush.asm`, `tss.c`               | Kernel + ring-3 code/data segments, TSS for RSP0 on ring3→ring0 |
| IDT / PIC     | `arch/x86_64/boot/idt.c`, `isr_stubs.asm`                        | All 256 vectors; 32 exceptions + 16 IRQs + syscall gate (0x80, DPL=3) |
| Timer         | `arch/x86_64/drivers/pit.c`                                     | PIT driving IRQ0; also drives the scheduler tick |
| Console       | `arch/x86_64/drivers/console.c`                                 | VGA text mode — superseded visually once a framebuffer is negotiated |
| Physical mem  | `mm/pmm.c`, `arch/x86_64/mm/multiboot2.c`                       | Bitmap frame allocator fed by the real Multiboot2 memory map |
| Kernel heap   | `mm/heap.c`                                                     | First-fit allocator (`kmalloc`/`kfree`), grows via contiguous PMM runs |
| Virtual mem   | `arch/x86_64/mm/vmm.c`, `paging.c`                              | Per-address-space page tables, deep-cloned from the kernel's; `paging.c` separately handles on-demand identity-mapping for MMIO (e.g. the framebuffer) |
| Keyboard      | `arch/x86_64/drivers/keyboard.c`                                | PS/2, scan code set 1, shift-aware |
| Mouse         | `arch/x86_64/drivers/mouse.c`                                   | PS/2, standard 3-byte packets |
| Graphics      | `arch/x86_64/drivers/framebuffer.c`, `font8x8.c`                | Linear RGB framebuffer (Multiboot2-negotiated), pixel/line/rect/text |
| GUI           | `gui/gui.c`                                                     | Arch-independent compositor: draggable titled windows, cursor, click-to-recolor |
| Scheduler     | `kernel/sched.c`, `arch/x86_64/sched.c`, `context_switch.asm`    | Preemptive round-robin kernel threads, timer-driven |
| Userspace     | `arch/x86_64/process.c`, `syscall.c`, `usermode.asm`             | Real ELF64 loader, ring-3 execution, `int 0x80` syscalls, isolated address space per process |
| Disk + FS     | `arch/x86_64/drivers/ata.c`, `fs/vfs.c`                          | ATA PIO (LBA28) + a tiny custom read-only filesystem, image built by `tools/mkfs.py` |

Unhandled CPU exceptions (divide-by-zero, GPF, page fault, etc.) call
`kernel_panic()` with the exception name and halt, rather than crashing
silently.

### Known limitations (honest, not hidden)

- **One process at a time.** `process_run()` is synchronous — it isn't
  threaded into `kernel/sched.c`'s ready queue yet. Launching/exiting reuses
  the same suspend/resume primitive as kernel threads
  (`arch_context_switch()`), just not scheduled concurrently with them.
- **No syscall pointer validation.** Arguments from ring 3 (e.g. a string
  pointer for `SYS_DEBUG_DRAW`) are dereferenced directly, not copied/
  validated. Safe today only because a process's address space is a full
  clone of the kernel's own; a real "copy from/to user" path is future work.
- **No huge-page splitting.** The VMM can't yet turn one of the boot
  identity map's 2MB pages into fine-grained 4KB entries, so anything that
  needs a real per-page mapping (like a user program) has to live outside
  the original 0–1GB identity-mapped range. `vmm_map()` recognizes this and
  safely no-ops rather than corrupting memory.
- **Filesystem is read-only, flat, and custom** — no subdirectories, no
  writes, not FAT/ext/anything-standard. It exists to prove the disk →
  driver → VFS chain works, not to read real-world disk images.
- Everything above is x86_64-only; `aarch64`/`arm32`/MCU targets have no
  code yet.

## Architecture abstraction

The core kernel (`kernel/kernel.c`, `kernel/sched.c`, `gui/gui.c`, `fs/vfs.c`,
`mm/pmm.c`, `mm/heap.c`) never touches hardware directly — it only calls the
functions declared in `include/arch/arch.h`:

```c
arch_name();                          // "x86_64", "aarch64", ...
arch_early_init();  arch_late_init(); // boot-time hooks
arch_console_init/putc/clear();
arch_keyboard_getchar();
arch_mouse_get_state(&x, &y, &buttons);
arch_gfx_available/width/height();
arch_gfx_put_pixel/fill_rect/draw_line/draw_string();
arch_interrupts_init/enable/disable();
arch_mm_init(magic, boot_info);  arch_get_total_ram();
arch_cpu_halt();  arch_cpu_relax();
arch_timer_init(hz);  arch_timer_ticks();
arch_thread_init_stack();  arch_context_switch();  // kernel thread mechanics
arch_disk_available();  arch_disk_read_sector();
```

Every architecture under `arch/<name>/` is expected to implement all of
these (returning "not available" where a capability doesn't exist yet is
fine — e.g. a keyboard-less board just always returns 0). Some deeper
mechanisms (virtual memory layout, ring transitions, ELF loading, ATA) are
still x86_64-specific modules rather than generic interfaces — they'll grow
a generic `arch.h` surface once a second architecture actually needs one,
same as graphics/mouse/threading did. Fixed-width types
(`include/kernel/types.h`) are used everywhere instead of `int`/`long`,
since their size isn't consistent across the target range.

## Project layout

```
arch/                 Per-architecture code (boot, GDT/IDT/TSS, mm, drivers)
  x86_64/              Implemented — see table above
  aarch64/, arm32/     Scaffolding only
  mcu/rp2040/          )
  mcu/stm32f4/         )  Scaffolding only
include/               Public headers (kernel/, arch/, mm/, gui/, fs/)
kernel/                Arch-independent core: kernel_main, panic, sched.c
mm/                    Arch-independent memory: pmm.c, heap.c
gui/                   Arch-independent windowing compositor
fs/                    Arch-independent VFS
drivers/               (not yet populated — arch-independent drivers go here)
lib/                   (not yet populated — freestanding libc-ish helpers)
userland/              Userspace test programs (hello.c), built as real ELF64
                       binaries and embedded into the kernel image
fsroot/                Files packed into the demo filesystem image by mkfs.py
tools/
  check-toolchain.sh   Checks that the expected cross-toolchains/QEMU are installed
  mkfs.py              Builds the disk image fs/vfs.c reads, from fsroot/
```

## Building & running

Requires, per target architecture:

- A `<target>-elf-gcc` cross-compiler (e.g. `x86_64-elf-gcc`)
- `nasm` (x86_64 only)
- `qemu-system-<arch>`
- `python3` (x86_64 only — builds the disk image)
- For bootable ISOs: a GRUB build that includes the **`i386-pc`** platform
  files (`grub-mkrescue`). Note: Homebrew's `x86_64-elf-grub` only ships the
  `x86_64-efi` platform and cannot produce a BIOS-bootable ISO — use
  `i686-elf-grub` instead (`brew install i686-elf-grub`).

Run `tools/check-toolchain.sh` to verify what's installed.

```sh
make                 # build x86_64 kernel.elf (default ARCH=x86_64)
make iso             # package it into a bootable ISO with GRUB2
make run             # build + launch in QEMU (kernel, ISO, and disk image)
make clean           # remove build/
make all-archs       # build every arch (aarch64/arm32 will no-op until implemented)
```

`make run` boots in QEMU under `-machine pc` (i440fx — **not** q35: q35 has
no legacy IDE controller at the classic ports by default, only AHCI/SATA,
and the ATA driver needs it) with a second drive attached for the demo
filesystem. You should see GRUB's menu, then the GUI come up with two
draggable windows, a mouse cursor, a blinking corner indicator (proving the
scheduler runs kernel threads concurrently), and — briefly, before the GUI's
first redraw overwrites it — boot-time status text confirming the VMM
self-test, the embedded ring-3 test program's exit code, and a line read
live from the demo disk file.

## Known gaps / next steps

- Real multi-process scheduling (processes as schedulable entities in
  `kernel/sched.c`'s ready queue, not one synchronous launch at a time).
- Syscall pointer validation / copy-from-user.
- Huge-page splitting in the VMM.
- A real filesystem (or at least subdirectories/writes on the current one),
  and a disk driver that isn't PIO-polling-only.
- `aarch64`, `arm32`, and the MCU targets have no code — everything above is
  x86_64-only so far. This is the project's actual next frontier: proving
  the `arch.h` abstraction holds up on hardware nothing like x86_64, not
  going further/deeper on x86_64 alone.
