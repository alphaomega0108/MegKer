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

**aarch64 now boots, brings up memory, and preemptively schedules kernel
threads**, targeting QEMU's `virt` machine: it boots straight from QEMU's
ELF loader (no bootloader stage), drops from whatever EL it resets into
down to EL1, brings up a PL011 console and memory (PMM + heap), then a
GICv2 + ARM Generic Timer drive the same arch-independent round-robin
scheduler x86_64 uses — verified by a second kernel thread actually
interleaving with the boot thread over serial output. No graphics, input,
or disk yet — see "What's implemented" below for the exact line.

`arm32` and MCU targets (`rp2040`, `stm32f4`) still exist as empty
scaffolding in `arch/` and in the Makefile, with no code yet.

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

### aarch64 boot sequence

1. QEMU's `-kernel` loader reads `kernel.elf` as a real ELF (not a Linux
   `Image`) and places segments directly at their linked addresses — no
   bootloader or firmware stage runs first — then jumps to `_start`
   (`arch/aarch64/boot/boot.S`) with a device-tree blob pointer in `x0`.
2. `_start` parks every core except CPU 0, reads `CurrentEL`, and drops
   from EL3 or EL2 (whichever QEMU reset into) down to EL1 via a
   `spsr`/`elr` + `eret` sequence — this kernel only targets EL1.
3. Once at EL1, it sets up a boot stack, zeros the C `.bss` region, and
   calls `kernel_main(0, dtb_ptr)` — aarch64 has no Multiboot-style magic,
   so `boot_magic` is unused; `boot_info` carries the (currently unparsed)
   DTB pointer instead.
4. `kernel_main()` — the same arch-independent entry point x86_64 uses —
   brings up console, memory, interrupts, and the timer, then
   `sched_init()`/`thread_create()` spin up a second kernel thread exactly
   as they do on x86_64. Since `arch_gfx_available()` is false on this arch,
   the idle loop polls `arch_keyboard_getchar()` (always 0 for now) instead
   of running the GUI, but the timer-driven scheduler tick keeps firing
   underneath it regardless.

### What's implemented (aarch64)

| Subsystem     | File(s)                                                          | Notes |
|---------------|-------------------------------------------------------------------|-------|
| Boot          | `arch/aarch64/boot/boot.S`, `linker.ld`                           | EL3/EL2→EL1 drop, targets QEMU's `virt` machine |
| Exceptions    | `arch/aarch64/boot/exceptions.S`                                  | Full VBAR_EL1 vector table; only IRQ (current EL, SPx) has a real handler — everything else still halts |
| Interrupts    | `arch/aarch64/drivers/gic.c`, `irq.c`                             | GICv2, Distributor + CPU interface; `GICC_CTLR.AckCtl` needed since EL1 reads as a Secure access with no EL3 firmware present (see commit for the debugging story) |
| Timer         | `arch/aarch64/drivers/timer.c`                                    | ARM Generic Timer, non-secure EL1 physical timer, PPI 30; drives the scheduler tick |
| Console       | `arch/aarch64/drivers/uart.c`                                     | PL011, polled, fixed MMIO base (`0x09000000`) |
| Physical mem  | `mm/pmm.c`, `arch/aarch64/arch.c`                                 | Same bitmap allocator as x86_64; RAM region is hardcoded, not parsed from the DTB |
| Kernel heap   | `mm/heap.c`                                                       | Identical arch-independent allocator — works unmodified since the MMU is off (physical == virtual) |
| Scheduler     | `kernel/sched.c`, `arch/aarch64/sched.c`, `boot/context_switch.S` | Same round-robin policy as x86_64; AArch64 context switch saves/restores x19–x30 per AAPCS64 |

Everything else in `arch.h` — graphics, keyboard/mouse, disk — is stubbed
to "not available" (`false`/`0`/no-op) rather than implemented.

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
- Everything in the table above is x86_64-only.
- **aarch64 has boot, memory, interrupts, and a preemptive scheduler, but
  no MMU/VMM, no userspace, no graphics, no keyboard/mouse, no disk.**
  The MMU is never enabled — kernel code runs with physical addressing
  throughout, so there's no address-space isolation yet (the x86_64
  equivalent of running everything the VMM stage added, minus the VMM).
  The RAM size is hardcoded to match the Makefile's `-m 1G` rather than
  parsed from the device tree QEMU hands in — a real DTB parser is future
  work, same spirit as x86_64's Multiboot2 memory-map walk.
- `arm32`/MCU targets have no code yet.

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
fine — e.g. a keyboard-less board just always returns 0). aarch64 is the
proof this holds up: `kernel/kernel.c` is byte-for-byte the same file on
both architectures, and it correctly skips the GUI/VFS paths on aarch64
purely because `arch_gfx_available()`/`arch_disk_available()` return
false — no `#ifdef ARCH_X86_64` anywhere in arch-independent code. Some
deeper mechanisms (virtual memory layout, ring transitions, ELF loading,
ATA) are still x86_64-specific modules rather than generic interfaces —
they'll grow a generic `arch.h` surface once a second architecture
actually needs one, same as graphics/mouse/threading did. Fixed-width
types (`include/kernel/types.h`) are used everywhere instead of
`int`/`long`, since their size isn't consistent across the target range.

## Project layout

```
arch/                 Per-architecture code (boot, GDT/IDT/TSS, mm, drivers)
  x86_64/              Implemented — see table above
  aarch64/             Boot + console + memory — see table above
  arm32/               Scaffolding only
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

- A `<target>-elf-gcc` cross-compiler (e.g. `x86_64-elf-gcc`, `aarch64-elf-gcc`)
- `nasm` (x86_64 only — aarch64 assembles `.S` files with `aarch64-elf-gcc` itself)
- `qemu-system-<arch>`
- `python3` (x86_64 only — builds the disk image)
- For bootable ISOs: a GRUB build that includes the **`i386-pc`** platform
  files (`grub-mkrescue`). Note: Homebrew's `x86_64-elf-grub` only ships the
  `x86_64-efi` platform and cannot produce a BIOS-bootable ISO — use
  `i686-elf-grub` instead (`brew install i686-elf-grub`).

Run `tools/check-toolchain.sh` to verify what's installed.

```sh
make                     # build x86_64 kernel.elf (default ARCH=x86_64)
make iso                 # package it into a bootable ISO with GRUB2
make run                 # build + launch in QEMU (kernel, ISO, and disk image)
make ARCH=aarch64 run    # build + launch aarch64 in QEMU's `virt` machine
make clean               # remove build/
make all-archs           # build every arch (arm32 will no-op until implemented)
```

`make run` (x86_64) boots in QEMU under `-machine pc` (i440fx — **not** q35:
q35 has no legacy IDE controller at the classic ports by default, only
AHCI/SATA, and the ATA driver needs it) with a second drive attached for the
demo filesystem. You should see GRUB's menu, then the GUI come up with two
draggable windows, a mouse cursor, a blinking corner indicator (proving the
scheduler runs kernel threads concurrently), and — briefly, before the GUI's
first redraw overwrites it — boot-time status text confirming the VMM
self-test, the embedded ring-3 test program's exit code, and a line read
live from the demo disk file.

`make ARCH=aarch64 run` boots straight into the kernel with no bootloader
stage (QEMU's `-kernel` loads the ELF directly) and prints the boot banner
over serial — with no graphics yet, there's nothing on the QEMU display
window itself, so `-display none` is the default in `QFLAGS`; watch the
terminal instead.

## Known gaps / next steps

x86_64:
- Real multi-process scheduling (processes as schedulable entities in
  `kernel/sched.c`'s ready queue, not one synchronous launch at a time).
- Syscall pointer validation / copy-from-user.
- Huge-page splitting in the VMM.
- A real filesystem (or at least subdirectories/writes on the current one),
  and a disk driver that isn't PIO-polling-only.

aarch64 (the actual next frontier — proving `arch.h` on hardware nothing
like x86_64):
- AArch64 MMU/VMM: TTBR0/1_EL1, a 4-level 4KB-granule page table walker,
  and per-address-space isolation — x86_64's `vmm_selftest()` pattern,
  translated to AArch64's page table format.
- EL0 userspace: SVC syscalls, dropping to EL0 via `eret` (the same
  mechanism `boot.S` already uses for EL-drops, aimed one level lower),
  and reusing the existing ELF64 loader logic.
- A virtio-mmio transport + virtqueue layer — QEMU's `virt` machine has no
  ATA/PS2/VGA equivalents, so disk (virtio-blk), graphics (virtio-gpu or
  `ramfb`), and input (virtio-input) all need this shared groundwork
  first, unlike x86_64 where each device had its own simple port-I/O or
  MMIO interface.
- A real device-tree parser, so RAM size comes from what QEMU actually
  reports instead of a hardcoded constant matched to the Makefile's `-m`
  flag.
- `arm32` and the MCU targets still have no code at all.
