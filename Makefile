# ═══════════════════════════════════════════════════════
# MegKer Master Makefile
# Usage:
#   make                    → build x86_64 (default)
#   make ARCH=aarch64       → build aarch64
#   make run                → build + launch in QEMU
#   make clean              → remove build outputs
#   make all-archs          → build all architectures
# ═══════════════════════════════════════════════════════

ARCH ?= x86_64

# ── Output paths ────────────────────────────────────────
# Defined before the toolchain block below: QFLAGS (per-arch) expands
# $(KERNEL)/$(ISO)/$(DISK_IMG) with `:=` (immediate expansion), so
# they must already exist at that point or QEMU gets invoked with an
# empty -kernel/-drive argument.
BUILD    := build/$(ARCH)
KERNEL   := $(BUILD)/kernel.elf
ISO      := $(BUILD)/megker.iso
DISK_IMG := $(BUILD)/disk.img

# ── Toolchain ───────────────────────────────────────────
ifeq ($(ARCH), x86_64)
    CC      := x86_64-elf-gcc
    AS      := nasm
    LD      := x86_64-elf-ld
    QEMU    := qemu-system-x86_64
    ASFLAGS := -f elf64
    CFLAGS  := -ffreestanding -fno-stack-protector \
               -fno-builtin -nostdlib -nostdinc    \
               -mno-red-zone -mcmodel=kernel       \
               -Wall -Wextra -std=c11              \
               -Iinclude
    LDFLAGS := -T arch/x86_64/linker.ld            \
               -z max-page-size=0x1000
    QFLAGS  := -machine pc                         \
               -m 256M                             \
               -serial stdio                       \
               -no-reboot                          \
               -no-shutdown                        \
               -drive format=raw,file=$(ISO)

else ifeq ($(ARCH), aarch64)
    CC      := aarch64-elf-gcc
    AS      := aarch64-elf-gcc
    LD      := aarch64-elf-ld
    QEMU    := qemu-system-aarch64
    ASFLAGS := -x assembler-with-cpp -ffreestanding -c -Iinclude
    CFLAGS  := -ffreestanding -fno-builtin         \
               -fno-stack-protector                \
               -nostdlib -nostdinc                 \
               -mcpu=cortex-a53 -mgeneral-regs-only \
               -Wall -Wextra -std=c11              \
               -Iinclude
    LDFLAGS := -T arch/aarch64/linker.ld
    # force-legacy=false: QEMU's virtio-mmio devices default to the
    # legacy (version 1) interface; arch/aarch64/drivers/virtio.c only
    # speaks the modern (version 2) one.
    QFLAGS  := -machine virt                       \
               -cpu cortex-a53                     \
               -m 1G                               \
               -serial stdio                       \
               -no-reboot                          \
               -no-shutdown                        \
               -display none                       \
               -global virtio-mmio.force-legacy=false \
               -kernel $(KERNEL)                   \
               -drive file=$(DISK_IMG),if=none,format=raw,id=hd0 \
               -device virtio-blk-device,drive=hd0

else ifeq ($(ARCH), arm32)
    CC      := arm-none-eabi-gcc
    AS      := arm-none-eabi-as
    LD      := arm-none-eabi-ld
    QEMU    := qemu-system-arm
    CFLAGS  := -ffreestanding -fno-builtin         \
               -nostdlib -nostdinc                 \
               -mcpu=cortex-a7                    \
               -Wall -Wextra -std=c11              \
               -Iinclude
    LDFLAGS := -T arch/arm32/linker.ld
    QFLAGS  := -machine raspi2b                    \
               -m 512M                             \
               -serial stdio                       \
               -kernel $(KERNEL)
endif

# ── Source files ────────────────────────────────────────

# Architecture specific .c files
ARCH_C_SRCS := $(shell find arch/$(ARCH)/ -name "*.c" 2>/dev/null)

# Architecture specific assembly (.asm — NASM, x86_64; .S — GNU as w/ cpp, aarch64/arm32)
ARCH_ASM_SRCS := $(shell find arch/$(ARCH)/ \( -name "*.asm" -o -name "*.S" \) 2>/dev/null)

# Common kernel .c files
KERN_SRCS := $(shell find kernel/ mm/ lib/ gui/ fs/ -name "*.c" 2>/dev/null)

# All object files
ARCH_C_OBJS   := $(patsubst %.c,   $(BUILD)/%.o, $(ARCH_C_SRCS))
ARCH_ASM_OBJS := $(patsubst %.asm, $(BUILD)/%.o, $(filter %.asm, $(ARCH_ASM_SRCS)))
ARCH_ASM_OBJS += $(patsubst %.S,   $(BUILD)/%.o, $(filter %.S,   $(ARCH_ASM_SRCS)))
KERN_OBJS     := $(patsubst %.c,   $(BUILD)/%.o, $(KERN_SRCS))

# Userland test program (x86_64 and aarch64 — both have a real
# userspace) — a real static ELF64 binary, embedded as raw data so
# each arch's ELF loader has something genuine to load rather than a
# hardcoded stub. Each arch's syscall ABI differs (int 0x80 vs. svc),
# so the source itself is arch-specific too.
ifeq ($(ARCH), x86_64)
    USERLAND_SRC   := userland/hello.c
    USERLAND_ELF   := $(BUILD)/userland/hello.elf
    USERLAND_EMBED := $(BUILD)/userland/hello_embed.o
else ifeq ($(ARCH), aarch64)
    USERLAND_SRC   := userland/hello_aarch64.c
    USERLAND_ELF   := $(BUILD)/userland/hello.elf
    USERLAND_EMBED := $(BUILD)/userland/hello_embed.o
else
    USERLAND_EMBED :=
endif

ALL_OBJS := $(ARCH_ASM_OBJS) $(ARCH_C_OBJS) $(KERN_OBJS) $(USERLAND_EMBED)

# ── Targets ─────────────────────────────────────────────
.PHONY: all run iso clean clean-all all-archs help

all: $(KERNEL)
	@echo ""
	@echo "  ✓ MegKer built successfully!"
	@echo "  Arch   : $(ARCH)"
	@echo "  Kernel : $(KERNEL)"
	@echo ""

# ── Link kernel ELF ─────────────────────────────────────
$(KERNEL): $(ALL_OBJS)
	@mkdir -p $(BUILD)
	@echo "  LD  $@"
	$(LD) $(LDFLAGS) -o $@ $^

# ── Compile .c files ────────────────────────────────────
$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "  CC  $<"
	$(CC) $(CFLAGS) -c $< -o $@

# ── Assemble .asm files (NASM, x86_64) ───────────────────
$(BUILD)/%.o: %.asm
	@mkdir -p $(dir $@)
	@echo "  AS  $<"
	$(AS) $(ASFLAGS) $< -o $@

# ── Assemble .S files (GNU as w/ cpp, aarch64/arm32) ─────
$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	@echo "  AS  $<"
	$(AS) $(ASFLAGS) $< -o $@

# ── Userland test program: build as a real static ELF64, then embed
#    its raw bytes as a linkable object (symbols _binary_hello_elf_*)
ifeq ($(ARCH), x86_64)
# Linked well above 1GB (0x40010000, with margin — the linker
# reserves a page before .text for the ELF/program headers, so
# -Ttext=0x40000000 exactly would still put the segment's actual
# start one page *below* 1GB) — deliberately past the boot identity
# map's 0-1GB range of 2MB huge pages, which the VMM can't yet split
# for a fine-grained user mapping.
$(USERLAND_ELF): $(USERLAND_SRC)
	@mkdir -p $(dir $@)
	@echo "  CC  $< (userland, ring 3)"
	x86_64-elf-gcc -ffreestanding -fno-stack-protector -fno-pie -no-pie \
		-nostdlib -static -std=c11 -Wall -Wextra \
		-Wl,--entry=_start -Wl,-Ttext=0x40010000 \
		-o $@ $<

$(USERLAND_EMBED): $(USERLAND_ELF)
	@echo "  LD  $< (embed)"
	cd $(dir $@) && x86_64-elf-ld -r -b binary -o $(notdir $@) $(notdir $<)

else ifeq ($(ARCH), aarch64)
# Linked at 0xC0010000 (3GB + 64KB) — past both of the boot identity
# map's two 1GB L1 blocks (0-1GB device, 1-2GB RAM), which the VMM
# can't yet split for a fine-grained user mapping.
$(USERLAND_ELF): $(USERLAND_SRC)
	@mkdir -p $(dir $@)
	@echo "  CC  $< (userland, EL0)"
	aarch64-elf-gcc -ffreestanding -fno-stack-protector -fno-pie -no-pie \
		-mgeneral-regs-only -nostdlib -static -std=c11 -Wall -Wextra \
		-Wl,--entry=_start -Wl,-Ttext=0xC0010000 \
		-o $@ $<

$(USERLAND_EMBED): $(USERLAND_ELF)
	@echo "  LD  $< (embed)"
	cd $(dir $@) && aarch64-elf-ld -r -b binary -o $(notdir $@) $(notdir $<)
endif

# ── Disk image: a tiny read-only filesystem (see fs/vfs.c) built by
#    tools/mkfs.py from whatever's in fsroot/, attached as a second
#    QEMU drive (the CD-ROM is the boot device, this is data only).
$(DISK_IMG): tools/mkfs.py fsroot/hello.txt
	@mkdir -p $(dir $@)
	@echo "  MKFS $@"
	python3 tools/mkfs.py $@ fsroot/hello.txt

# ── Make bootable ISO (needs grub) ──────────────────────
iso: $(KERNEL)
	@mkdir -p $(BUILD)/iso/boot/grub
	@cp $(KERNEL) $(BUILD)/iso/boot/kernel.elf
	@echo 'set timeout=3'                          > $(BUILD)/iso/boot/grub/grub.cfg
	@echo 'set default=0'                         >> $(BUILD)/iso/boot/grub/grub.cfg
	@echo 'menuentry "MegKer" {'                  >> $(BUILD)/iso/boot/grub/grub.cfg
	@echo '    multiboot2 /boot/kernel.elf'       >> $(BUILD)/iso/boot/grub/grub.cfg
	@echo '    boot'                              >> $(BUILD)/iso/boot/grub/grub.cfg
	@echo '}'                                     >> $(BUILD)/iso/boot/grub/grub.cfg
	i686-elf-grub-mkrescue -o $(ISO) $(BUILD)/iso
	@echo "  ✓ ISO created: $(ISO)"

# ── Run in QEMU ─────────────────────────────────────────
ifeq ($(ARCH), x86_64)
# Machine type is `pc` (i440fx), not q35: q35 has no legacy IDE
# controller at the classic ports by default (AHCI/SATA only), which
# the ATA PIO driver (arch/x86_64/drivers/ata.c) needs for the disk.
run: iso $(DISK_IMG)
	@echo "  Launching QEMU..."
	$(QEMU) \
		-machine pc \
		-m 256M \
		-serial stdio \
		-no-reboot \
		-no-shutdown \
		-drive file=$(DISK_IMG),format=raw,if=ide \
		-cdrom $(BUILD)/megker.iso \
		-boot d
else
# Other arches boot the ELF directly via QEMU's -kernel (no
# bootloader/ISO stage) — see QFLAGS above.
run: $(KERNEL) $(DISK_IMG)
	@echo "  Launching QEMU..."
	$(QEMU) $(QFLAGS)
endif

# ── Build all architectures ─────────────────────────────
all-archs:
	@$(MAKE) ARCH=x86_64
	@$(MAKE) ARCH=aarch64
	@$(MAKE) ARCH=arm32
	@echo ""
	@echo "  ✓ All architectures built!"

# ── Clean ───────────────────────────────────────────────
clean:
	@rm -rf $(BUILD)
	@echo "  ✓ Cleaned $(BUILD)"

clean-all:
	@rm -rf build/
	@echo "  ✓ Cleaned all architectures"

# ── Help ────────────────────────────────────────────────
help:
	@echo ""
	@echo "  MegKer Build System"
	@echo ""
	@echo "  make                   build x86_64 kernel"
	@echo "  make ARCH=aarch64      build aarch64 kernel"
	@echo "  make ARCH=arm32        build arm32 kernel"
	@echo "  make run               build + run in QEMU"
	@echo "  make iso               create bootable ISO"
	@echo "  make all-archs         build all targets"
	@echo "  make clean             remove build output for the current ARCH"
	@echo "  make clean-all         remove build output for every ARCH"
	@echo ""