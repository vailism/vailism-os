# Vailism OS

**Vailism OS** is a custom 64-bit (x86_64) operating system built from scratch in C and Assembly.

---

## 🌟 Features (Phase 1 — Bootable Kernel)

- **Architecture**: x86_64 Long Mode (Ring 0 / Supervisor Mode).
- **Higher-Half Kernel**: Virtual address space mapped at `0xffffffff80000000`.
- **Bootloader**: Modern Limine boot protocol (v8.x specification) supporting both BIOS and UEFI.
- **Serial Debug Driver**: 16550 UART COM1 (`0x3F8`) driver at 38400 baud.
- **Linear Graphical Framebuffer**: 32-bit ARGB/XRGB color rendering with custom 8x16 VGA font engine, text cursor tracking, and terminal scrolling.
- **Cross-Platform Toolchain**: Fully buildable and testable on **macOS (Apple Silicon M-Series)** and Linux with zero host OS modifications.

---

## 🛠️ Project Structure

```
.
├── Makefile                # Cross-compilation & ISO build rules
├── limine.conf             # Bootloader menu configuration
├── linker.ld               # Memory layout & higher-half mapping
├── limine-bin/             # Limine bootloader assets (BIOS + UEFI)
└── src/
    ├── include/
    │   ├── font8x16.h      # Embedded 8x16 VGA bitmap font table
    │   ├── framebuffer.h   # Linear RGB graphics & terminal header
    │   ├── io.h            # Port I/O inline assembly (inb, outb)
    │   ├── limine.h        # Official Limine boot protocol specification
    │   ├── serial.h        # 16550 UART Serial driver header
    │   └── types.h         # Standard integer & boolean definitions
    └── kernel/
        ├── framebuffer.c   # Screen clearing, pixel plotting, font rendering
        ├── main.c          # Kernel entry point (kmain)
        └── serial.c        # COM1 port initialization & string output
```

---

## 🚀 Building & Running

### Prerequisites (macOS)
```bash
brew install llvm lld nasm qemu xorriso mtools
```

### Build & Run in QEMU
```bash
# Compile kernel, package bootable ISO, and launch in QEMU
make run
```

### Headless / Serial-only Mode
```bash
make run-nographic
```

---

## 🗺️ Roadmap

- [x] **Phase 1**: Bootable Kernel (Limine, 64-bit Entry, Serial COM1, Framebuffer Terminal)
- [ ] **Phase 2**: CPU & Interrupts (GDT, IDT, Exceptions, PIC/APIC, PIT Timer, Keyboard)
- [ ] **Phase 3**: Memory Management (PMM, Paging, VMM, Kernel Heap malloc/free)
- [ ] **Phase 4**: Multitasking (Processes, Threads, Context Switching, Preemptive Scheduler)
- [ ] **Phase 5**: Storage & Filesystem (Disk Drivers, VFS, Ext2/FAT, File Descriptors)
- [ ] **Phase 6**: Userland (Syscalls, User Mode Ring 3, LibC, Interactive Shell)
- [ ] **Phase 7**: GUI & Desktop Environment (Window Manager, Compositor, Mouse Driver)

---

## 📄 License
MIT License
