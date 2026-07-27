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
    QFLAGS  := -machine q35                        \
               -m 256M                             \
               -serial stdio                       \
               -no-reboot                          \
               -no-shutdown                        \
               -drive format=raw,file=$(ISO)

else ifeq ($(ARCH), aarch64)
    CC      := aarch64-elf-gcc
    AS      := aarch64-elf-as
    LD      := aarch64-elf-ld
    QEMU    := qemu-system-aarch64
    CFLAGS  := -ffreestanding -fno-builtin         \
               -nostdlib -nostdinc                 \
               -mcpu=cortex-a53                   \
               -Wall -Wextra -std=c11              \
               -Iinclude
    LDFLAGS := -T arch/aarch64/linker.ld
    QFLAGS  := -machine raspi3b                    \
               -m 1G                               \
               -serial stdio                       \
               -kernel $(KERNEL)

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

# ── Output paths ────────────────────────────────────────
BUILD   := build/$(ARCH)
KERNEL  := $(BUILD)/kernel.elf
ISO     := $(BUILD)/megker.iso

# ── Source files ────────────────────────────────────────

# Architecture specific .c files
ARCH_C_SRCS := $(shell find arch/$(ARCH)/ -name "*.c" 2>/dev/null)

# Architecture specific .asm files (x86_64 only)
ARCH_ASM_SRCS := $(shell find arch/$(ARCH)/ -name "*.asm" 2>/dev/null)

# Common kernel .c files
KERN_SRCS := $(shell find kernel/ mm/ lib/ gui/ -name "*.c" 2>/dev/null)

# All object files
ARCH_C_OBJS   := $(patsubst %.c,   $(BUILD)/%.o, $(ARCH_C_SRCS))
ARCH_ASM_OBJS := $(patsubst %.asm, $(BUILD)/%.o, $(ARCH_ASM_SRCS))
KERN_OBJS     := $(patsubst %.c,   $(BUILD)/%.o, $(KERN_SRCS))

ALL_OBJS := $(ARCH_ASM_OBJS) $(ARCH_C_OBJS) $(KERN_OBJS)

# ── Targets ─────────────────────────────────────────────
.PHONY: all run iso clean all-archs help

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

# ── Assemble .asm files (NASM) ───────────────────────────
$(BUILD)/%.o: %.asm
	@mkdir -p $(dir $@)
	@echo "  AS  $<"
	$(AS) $(ASFLAGS) $< -o $@

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
run: iso
	@echo "  Launching QEMU..."
	$(QEMU) \
		-machine q35 \
		-m 256M \
		-serial stdio \
		-no-reboot \
		-no-shutdown \
		-cdrom $(BUILD)/megker.iso \
		-boot d

# ── Build all architectures ─────────────────────────────
all-archs:
	@$(MAKE) ARCH=x86_64
	@$(MAKE) ARCH=aarch64
	@$(MAKE) ARCH=arm32
	@echo ""
	@echo "  ✓ All architectures built!"

# ── Clean ───────────────────────────────────────────────
clean:
	@rm -rf build/
	@echo "  ✓ Cleaned"

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
	@echo "  make clean             remove build output"
	@echo ""