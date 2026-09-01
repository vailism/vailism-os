# Vailism OS Build System (macOS Apple Silicon & Linux Cross-Compile)

CC = /opt/homebrew/opt/llvm/bin/clang
LD = /opt/homebrew/bin/ld.lld
NASM = /opt/homebrew/bin/nasm
QEMU = /opt/homebrew/bin/qemu-system-x86_64
XORRISO = /opt/homebrew/bin/xorriso

# Fallback if installed in standard PATH
ifeq ($(wildcard $(CC)),)
    CC = clang
endif
ifeq ($(wildcard $(LD)),)
    LD = ld.lld
endif
ifeq ($(wildcard $(NASM)),)
    NASM = nasm
endif
ifeq ($(wildcard $(QEMU)),)
    QEMU = qemu-system-x86_64
endif
ifeq ($(wildcard $(XORRISO)),)
    XORRISO = xorriso
endif

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

.PHONY: all clean iso run run-nographic

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

# Run OS in QEMU with GUI window and serial output forwarded to terminal
run: $(ISO)
	@echo "Starting Vailism OS in QEMU (Press Ctrl+C in terminal or close window to exit)..."
	$(QEMU) -cdrom $(ISO) -serial stdio -m 512M -vga std

# Run OS in headless/nographic mode (serial only)
run-nographic: $(ISO)
	@echo "Starting Vailism OS in headless QEMU (Ctrl+A then X to exit)..."
	$(QEMU) -cdrom $(ISO) -nographic -m 512M

clean:
	@rm -rf $(BUILD_DIR)
	@echo "Cleaned build artifacts."
