# ==============================================================================
# Vailism OS Build System
# Native macOS (Apple Silicon M1/M2/M3/M4 & Intel) and Linux Cross-Compilation
# ==============================================================================

# Detect Host OS and Architecture
HOST_OS   := $(shell uname -s)
HOST_ARCH := $(shell uname -m)

# Toolchain discovery for macOS (Apple Silicon & Intel) and Linux
ifeq ($(HOST_OS),Darwin)
    ifeq ($(HOST_ARCH),arm64)
        BREW_PREFIX ?= /opt/homebrew
    else
        BREW_PREFIX ?= /usr/local
    endif

    CC      := $(shell which $(BREW_PREFIX)/opt/llvm/bin/clang $(BREW_PREFIX)/bin/clang clang 2>/dev/null | head -n 1)
    LD      := $(shell which $(BREW_PREFIX)/bin/ld.lld $(BREW_PREFIX)/opt/llvm/bin/ld.lld ld.lld 2>/dev/null | head -n 1)
    NASM    := $(shell which $(BREW_PREFIX)/bin/nasm nasm 2>/dev/null | head -n 1)
    QEMU    := $(shell which $(BREW_PREFIX)/bin/qemu-system-x86_64 qemu-system-x86_64 2>/dev/null | head -n 1)
    XORRISO := $(shell which $(BREW_PREFIX)/bin/xorriso xorriso 2>/dev/null | head -n 1)

    QEMU_DISPLAY_FLAGS := -display cocoa,zoom-to-fit=on
else
    CC      := clang
    LD      := ld.lld
    NASM    := nasm
    QEMU    := qemu-system-x86_64
    XORRISO := xorriso

    QEMU_DISPLAY_FLAGS := -display gtk,zoom-to-fit=on
endif

# Cross-compilation flags for bare-metal x86_64 ELF
CFLAGS = --target=x86_64-unknown-none-elf \
         -std=c11 \
         -ffreestanding \
         -fno-stack-protector \
         -fno-stack-check \
         -fno-lto \
         -fno-pic \
         -mno-red-zone \
         -mno-80387 \
         -mno-mmx \
         -mno-sse \
         -mno-sse2 \
         -mcmodel=kernel \
         -Wall \
         -Wextra \
         -O2 \
         -Isrc/include

LDFLAGS = -nostdlib \
          -static \
          -T linker.ld \
          -z max-page-size=0x1000

BUILD_DIR = build
ISO_DIR   = $(BUILD_DIR)/iso_root
KERNEL    = $(BUILD_DIR)/vailism-kernel.elf
ISO       = $(BUILD_DIR)/vailism-os.iso

C_SRCS   = $(wildcard src/kernel/*.c)
ASM_SRCS = $(wildcard src/arch/x86_64/*.asm)

C_OBJS   = $(patsubst src/kernel/%.c, $(BUILD_DIR)/%.o, $(C_SRCS))
ASM_OBJS = $(patsubst src/arch/x86_64/%.asm, $(BUILD_DIR)/%_asm.o, $(ASM_SRCS))
ALL_OBJS = $(C_OBJS) $(ASM_OBJS)

.PHONY: all clean iso run run-fullscreen run-nographic setup-mac help

all: $(ISO)

# Compile C sources into 64-bit ELF objects
$(BUILD_DIR)/%.o: src/kernel/%.c
	@mkdir -p $(BUILD_DIR)
	@echo " [CC] $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Assemble x86_64 assembly sources with NASM
$(BUILD_DIR)/%_asm.o: src/arch/x86_64/%.asm
	@mkdir -p $(BUILD_DIR)
	@echo " [NASM] $<"
	@$(NASM) -f elf64 $< -o $@

# Link kernel into 64-bit ELF executable
$(KERNEL): $(ALL_OBJS) linker.ld
	@echo " [LD] $@"
	@$(LD) $(LDFLAGS) $(ALL_OBJS) -o $@

# Build bootable ISO using xorriso and Limine
$(ISO): $(KERNEL) limine.conf
	@echo " [ISO] Creating bootable ISO: $@"
	@mkdir -p $(ISO_DIR)/boot
	@mkdir -p $(ISO_DIR)/boot/limine
	@mkdir -p $(ISO_DIR)/EFI/BOOT
	@cp $(KERNEL) $(ISO_DIR)/boot/
	@cp limine.conf $(ISO_DIR)/boot/limine/
	@cp limine-bin/limine-bios.sys $(ISO_DIR)/boot/limine/
	@cp limine-bin/limine-bios-cd.bin $(ISO_DIR)/boot/limine/
	@cp limine-bin/limine-uefi-cd.bin $(ISO_DIR)/boot/limine/
	@cp limine-bin/BOOTX64.EFI $(ISO_DIR)/EFI/BOOT/
	@$(XORRISO) -as mkisofs -b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		$(ISO_DIR) -o $(ISO) > /dev/null 2>&1
	@./limine-bin/limine bios-install $(ISO) > /dev/null 2>&1
	@echo " [OK] Bootable ISO ready at $(ISO)"

iso: $(ISO)

# Run OS in QEMU with GUI window and serial output forwarded to terminal (scaled proportionally)
run: $(ISO)
	@echo "Starting Vailism OS in QEMU (Click inside window to capture trackpad/mouse, Ctrl+Alt+G to release)..."
	$(QEMU) -cdrom $(ISO) -serial stdio -m 512M -vga std $(QEMU_DISPLAY_FLAGS)

# Run OS in Fullscreen QEMU (scaled to entire display)
run-fullscreen: $(ISO)
	@echo "Starting Vailism OS in Fullscreen QEMU (Ctrl+Alt+G to release)..."
	$(QEMU) -cdrom $(ISO) -serial stdio -m 512M -vga std -full-screen $(QEMU_DISPLAY_FLAGS)

# Run OS in headless/nographic mode (serial only)
run-nographic: $(ISO)
	@echo "Starting Vailism OS in headless QEMU (Ctrl+A then X to exit)..."
	$(QEMU) -cdrom $(ISO) -nographic -m 512M

# Run automated macOS setup script
setup-mac:
	@chmod +x scripts/setup_macos.sh
	@./scripts/setup_macos.sh

clean:
	@rm -rf $(BUILD_DIR)
	@echo "Cleaned build artifacts."

help:
	@echo "Vailism OS Build Targets:"
	@echo "  make               - Build the bootable ISO ($(ISO))"
	@echo "  make run           - Run in QEMU with scaled window"
	@echo "  make run-fullscreen- Run in QEMU in fullscreen mode"
	@echo "  make run-nographic - Run headless (serial only in terminal)"
	@echo "  make setup-mac     - Install prerequisites on macOS via Homebrew"
	@echo "  make clean         - Remove build artifacts"
