# Vailism OS

> A 64-bit x86 operating system engineered from the ground up.

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Target: x86_64](https://img.shields.io/badge/Target-x86__64_Bare_Metal-informational.svg)]()
[![Bootloader: Limine](https://img.shields.io/badge/Bootloader-Limine_v8-brightgreen.svg)]()
[![Build: Clang + NASM](https://img.shields.io/badge/Toolchain-LLVM_Clang_%2B_NASM-orange.svg)]()

---

## 📌 About

**Vailism OS** is an experimental operating system written in C and x86_64 Assembly, developed with the goal of understanding — and controlling — the complete software stack between the processor and the user.

This is not a Linux distribution, desktop skin, or collection of shell scripts. The objective is to build the system itself from bare metal:

$$\text{Boot} \longrightarrow \text{CPU Initialization} \longrightarrow \text{Memory} \longrightarrow \text{Interrupts} \longrightarrow \text{Devices} \longrightarrow \text{Scheduling} \longrightarrow \text{Storage} \longrightarrow \text{System Calls} \longrightarrow \text{Userspace} \longrightarrow \text{Graphics}$$

### 🏷️ Topics / Tags
`operating-system` `osdev` `x86_64` `kernel` `bare-metal` `limine-bootloader` `assembly` `c` `qemu` `paging` `interrupts` `low-level`

---

## 1. System Architecture

Vailism is designed as a layered system. Hardware-dependent mechanisms remain below the kernel’s portable core, while userspace interacts with the machine through explicit kernel interfaces.

```
                                  VAILISM OS
                         x86_64 Operating System
                                   │
                                   ▼
                         ┌───────────────────┐
                         │  Physical Machine │
                         │    x86_64 CPU     │
                         └─────────┬─────────┘
                                   │
                              BIOS / UEFI
                                   │
                                   ▼
                         ┌───────────────────┐
                         │      Limine       │
                         │   Boot Protocol   │
                         └─────────┬─────────┘
                                   │
                 CPU state / memory map / framebuffer
                                   │
                                   ▼
                    ┌──────────────────────────┐
                    │      Kernel Entry        │
                    │         kmain            │
                    └────────────┬─────────────┘
                                 │
          ┌──────────────────────┼──────────────────────┐
          │                      │                      │
          ▼                      ▼                      ▼
 ┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
 │  Architecture   │    │     Drivers     │    │     Memory      │
 │     Layer       │    │                 │    │    Subsystem    │
 ├─────────────────┤    ├─────────────────┤    ├─────────────────┤
 │ GDT             │    │ Serial (COM1)   │    │ Physical Memory │
 │ IDT (256 gates) │    │ Framebuffer     │    │ Paging (4-Level)│
 │ Exceptions 0-31 │    │ Keyboard (PS/2) │    │ Virtual Memory  │
 │ IRQs (8259 PIC) │    │ Timer (8254 PIT)│    │ Kernel Heap     │
 │ CPU Control     │    │ Storage (ATA)   │    │ Allocators      │
 └────────┬────────┘    └────────┬────────┘    └────────┬────────┘
          │                      │                      │
          └──────────────────────┼──────────────────────┘
                                 │
                                 ▼
                    ┌──────────────────────────┐
                    │       Kernel Core        │
                    ├──────────────────────────┤
                    │ Scheduler                │
                    │ Processes / Threads      │
                    │ IPC                      │
                    │ VFS / Filesystems        │
                    │ System Call Interface    │
                    └────────────┬─────────────┘
                                 │
                           Ring 0 → Ring 3
                                 │
                                 ▼
                    ┌──────────────────────────┐
                    │        Userland          │
                    ├──────────────────────────┤
                    │ libc                     │
                    │ Shell                    │
                    │ Core Utilities           │
                    │ Applications             │
                    └────────────┬─────────────┘
                                 │
                                 ▼
                    ┌──────────────────────────┐
                    │       Vailism UI         │
                    ├──────────────────────────┤
                    │ Framebuffer              │
                    │ Compositor               │
                    │ Window Manager           │
                    │ Mouse / Input            │
                    │ Applications             │
                    └──────────────────────────┘
        ┌─────────────────────────────────────────────────┐
        │              DEVELOPMENT INFRASTRUCTURE         │
        ├─────────────────────────────────────────────────┤
        │ Cross Compiler │ Linker │ Make │ QEMU │ GDB     │
        │ Serial Debug  │ ISO    │ CI   │ Tests            │
        └─────────────────────────────────────────────────┘
```

---

## 2. Design Philosophy

Vailism is developed around clear engineering principles:

### Understand before abstracting
A subsystem should first exist in a form that makes its interaction with the hardware understandable. Abstractions are introduced when they solve a real architectural problem — not simply because a framework makes the code shorter.

### Kernel boundaries should be explicit
The kernel owns privileged operations. Userspace interacts with the kernel through defined interfaces rather than directly manipulating hardware.

```
USERSPACE
    │
    │ system call
    ▼
┌───────────────┐
│     KERNEL    │
├───────────────┤
│ Scheduler     │
│ Memory        │
│ Filesystem    │
│ Drivers       │
└───────┬───────┘
        │
        ▼
     HARDWARE
```

### Hardware is not an implementation detail
The project deliberately exposes the mechanisms normally hidden by modern operating systems:
- CPU privilege levels (Ring 0 vs Ring 3)
- 4-level page tables & translation lookaside buffers (TLB)
- Interrupt descriptors and hardware IRQs
- Physical memory frames and virtual address spaces
- Device registers and port I/O
- Context switching & register frames
- System calls & executable loading

---

## 3. Boot Sequence

The initial boot path is intentionally simple and deterministic:

```
Power On
   │
   ▼
Firmware (BIOS / UEFI)
   │
   ▼
Limine Bootloader
   ├── Load 64-bit ELF Kernel
   ├── Discover Physical Memory Map
   ├── Initialize Linear Graphical Framebuffer
   └── Enter 64-bit Long Mode
   │
   ▼
Vailism Kernel Entry (kmain)
   │
   ├── CPU & Architecture Initialization (GDT, TSS, IST)
   ├── Interrupt Subsystem (IDT, Exceptions 0-31, 8259 PIC)
   ├── Device Drivers (Serial COM1, Framebuffer, PIT Timer, PS/2 Keyboard)
   ├── Memory Management (PMM Bitmap, VMM 4-Level Paging, Kernel Heap)
   ├── Kernel Core Services (Scheduler, Multitasking)
   └── Userspace Transition (Ring 3)
```

The bootloader is kept outside the kernel’s responsibilities. Vailism consumes the standard Limine protocol and focuses on what happens after control reaches the kernel.

---

## 4. Current Implementation

### Architecture
- **Architecture**: x86_64 Long Mode (64-bit).
- **Privilege Level**: Ring 0 (Supervisor Execution).
- **Virtual Memory Base**: Higher-half kernel mapped at `0xffffffff80000000`.

### Boot & Firmware Support
- **Protocol**: Limine Boot Protocol (v8 specification).
- **Firmware**: Dual boot support for both **UEFI** and legacy **BIOS**.

### Architecture & CPU Control
- **GDT (Global Descriptor Table)**: 64-bit Kernel Code (`0x08`), Kernel Data (`0x10`), User Data (`0x18`), User Code (`0x20`), and 64-bit TSS (`0x28`).
- **TSS (Task State Segment)**: Configured with `RSP0` kernel stack and dedicated `IST1` stack for safe Double Fault exception recovery.
- **IDT (Interrupt Descriptor Table)**: 256 interrupt gates handling all 32 CPU exceptions (`#DE`, `#DF`, `#GP`, `#PF`) with full CPU register state preservation (`struct registers`).
- **Interrupt Controller**: 8259 PIC remapped to vectors `0x20..0x2F` (IRQs 0..15).
- **Timer Subsystem**: 8254 PIT configured to 100 Hz (10 ms ticks) driving uptime tracking and `timer_sleep()`.
- **Keyboard Driver**: PS/2 controller driver on IRQ1 (Vector 33) decoding Scancode Set 1 with Shift/Caps Lock/Enter/Backspace support.

### Memory Management
- **Physical Memory Manager (PMM)**: Bitmap page frame allocator tracking 4 KiB frames across all physical RAM.
- **Virtual Memory Manager (VMM)**: 4-level x86_64 page tables (`PML4` $\rightarrow$ `PDPT` $\rightarrow$ `PD` $\rightarrow$ `PT`) with page mapping, unmapping, and TLB invalidation (`invlpg`).
- **Kernel Heap Allocator**: Coalescing free-list allocator providing `kmalloc()`, `kfree()`, `kcalloc()`, and `krealloc()` with 16-byte alignment and block splitting starting at `0xffffffffc0000000`.

### Dual Debugging & Graphics Channels
- **Serial Port (COM1 `0x3F8`)**: 16550 UART driver operating at 38400 baud. Serial output provides rock-solid diagnostics to the host terminal even during early kernel faults.
- **Graphical Framebuffer Console**: Direct VRAM linear framebuffer rendering 32-bit ARGB color with an embedded 8x16 VGA bitmap font engine, cursor tracking, and terminal scrolling.

---

## 5. Repository Structure

```
Vailism/
├── Makefile                # Cross-compilation & ISO packaging
├── linker.ld               # Memory layout & higher-half section mapping
├── limine.conf             # Bootloader menu configuration
├── limine-bin/             # Vendored Limine binary boot assets
│
├── src/
│   ├── arch/
│   │   └── x86_64/
│   │       ├── gdt_flush.asm   # GDT reload & 64-bit far return
│   │       └── interrupts.asm  # ISR & IRQ assembly stubs, context save/restore
│   │
│   ├── include/
│   │   ├── types.h         # Standard integer types (uint64_t, size_t, bool)
│   │   ├── io.h            # Port I/O inline assembly (inb, outb, io_wait)
│   │   ├── limine.h        # Official Limine boot protocol specification
│   │   ├── serial.h        # 16550 UART driver header
│   │   ├── framebuffer.h   # Linear RGB graphics & terminal header
│   │   ├── font8x16.h      # Embedded 8x16 VGA bitmap font table
│   │   ├── gdt.h           # GDT & TSS structures
│   │   ├── idt.h           # IDT descriptors & register state structs
│   │   ├── pic.h           # 8259 PIC controller definitions
│   │   ├── timer.h         # 8254 PIT timer driver header
│   │   ├── keyboard.h      # PS/2 keyboard driver header
│   │   ├── string.h        # Freestanding string & memory operations
│   │   ├── memory.h        # Page macros & address translation helpers
│   │   ├── pmm.h           # Physical Memory Manager header
│   │   ├── vmm.h           # Virtual Memory Manager & paging header
│   │   └── heap.h          # Kernel Heap allocator header
│   │
│   └── kernel/
│       ├── main.c          # Kernel entry point (kmain)
│       ├── serial.c        # Serial port driver implementation
│       ├── framebuffer.c   # Screen clearing, pixel plotting, font rendering
│       ├── gdt.c           # GDT and TSS initialization
│       ├── idt.c           # IDT gate setup and exception dispatcher
│       ├── pic.c           # 8259 PIC remapping and EOI signalling
│       ├── timer.c         # PIT timer tick handler and sleep functions
│       ├── keyboard.c      # PS/2 scancode decoder and interactive echo
│       ├── string.c        # Freestanding memset, memcpy, strlen, strcmp
│       ├── pmm.c           # Physical memory bitmap frame allocator
│       ├── vmm.c           # 4-level paging table walker & mapper
│       └── heap.c          # Dynamic kernel heap allocator implementation
│
├── build/                  # Generated build artifacts & bootable ISO
└── README.md
```

---

## 6. Development Environment

Vailism is developed and tested inside virtual machines. The host operating system is never modified during development.

```
┌──────────────────────────────────────┐
│          Host: macOS / Linux         │
│                                      │
│  Compiler → Linker → ISO Generation │
│                  │                   │
│                  ▼                   │
│                QEMU                  │
│                  │                   │
│                  ▼                   │
│            Vailism OS                │
└──────────────────────────────────────┘
```

### Prerequisites (macOS Apple Silicon / Intel)
```bash
brew install llvm lld nasm qemu xorriso mtools
```

### Build & Run Commands
```bash
# Build bootable ISO and launch in QEMU with graphical window & serial stdio
make run

# Build and run in headless mode (serial output only)
make run-nographic

# Clean build artifacts
make clean
```

---

## 7. Memory Model

Vailism utilizes a canonical higher-half kernel memory layout:

```
Virtual Address Space
──────────────────────────────────────────────
0xFFFFFFFFFFFFFFFF
        │
        │       Kernel Space (Code, Data, Heap)
        │
0xFFFFFFFFC0000000  ← Kernel Heap Base (kmalloc pool)
        │
0xFFFFFFFF80000000  ← Kernel Virtual Base (ELF Image)
        │
        │       Higher-Half Direct Map (HHDM)
        │
0xFFFF800000000000  ← All Physical RAM mapped 1:1
        │
        │       Unmapped / Guard Pages
        │
0x00007FFFFFFFFFFF  ← User Space Ceiling (Ring 3)
        │
        │       User Applications & Stack
        │
0x0000000000000000
```

The memory subsystem is divided into three distinct layers:

```
Physical Memory
      │
      ▼
Physical Memory Manager (PMM Bitmap)
      │
      ▼
Page Tables / Virtual Memory Manager (4-Level Paging)
      │
      ▼
Kernel Heap / Allocators (kmalloc / kfree)
```

---

## 8. Kernel Subsystems

```
                Applications
                     │
                     ▼
             System Call API
                     │
                     ▼
                   VFS
              ┌──────┴──────┐
              ▼             ▼
            FAT          Ext2
              │             │
              └──────┬──────┘
                     ▼
                Block Device
                     │
                     ▼
                  Hardware
```

- **Architecture Layer**: Handles x86_64 GDT, TSS, IDT, CPU exceptions, IRQs, APIC, context switching, and assembly stubs.
- **Memory Manager**: Controls physical page allocation, 4-level page tables, virtual address spaces, and kernel heap.
- **Scheduler**: Manages threads, processes, context switches, preemption, and scheduling policies.
- **Drivers**: Provides abstract interfaces to hardware (UART, Framebuffer, Keyboard, PIT Timer, Storage, Mouse).
- **Filesystem (VFS)**: Separates generic POSIX-like file operations (`open`, `read`, `write`, `close`) from concrete disk filesystem implementations (FAT32, Ext2).

---

## 9. User / Kernel Boundary

```
                 RING 3
        ┌────────────────────┐
        │ Shell              │
        │ Applications       │
        │ libc               │
        └─────────┬──────────┘
                  │
             SYSTEM CALL
                  │
                  ▼
                 RING 0
        ┌────────────────────┐
        │ Kernel             │
        │                    │
        │ Scheduler          │
        │ Memory Manager     │
        │ Filesystem         │
        │ Drivers            │
        └────────────────────┘
```

The privilege boundary is fundamental. Applications in Ring 3 cannot:
- Modify kernel memory or page tables
- Directly execute privileged CPU instructions (`cli`, `sti`, `invlpg`, `mov cr3, ...`)
- Directly manipulate device I/O ports
- Interfere with another process's memory space

All services are mediated through the **System Call Interface**.

---

## 10. Roadmap

- [x] **Phase 1 — Kernel Bring-Up**: Limine boot, x86_64 entry, higher-half mapping, 16550 UART serial driver, linear framebuffer console, 8x16 font renderer, QEMU runner.
- [x] **Phase 2 — CPU & Interrupts**: GDT, 64-bit TSS, IDT (256 gates), 32 CPU exception handlers, 8259 PIC remapping, 8254 PIT timer (100 Hz), PS/2 keyboard driver with interactive echo.
- [x] **Phase 3 — Memory Management**: Physical Memory Manager (PMM bitmap), 4-level paging Virtual Memory Manager (VMM), Kernel Heap allocator (`kmalloc`, `kfree`, `kcalloc`, `krealloc`).
- [x] **Phase 4 — Execution & Multitasking**: Process & Thread Control Blocks (PCB/TCB), CPU context switching (`context_switch`), preemptive timer-driven round-robin scheduler, non-blocking `thread_sleep()`, cooperative `yield()`, thread lifecycle management.
- [ ] **Phase 5 — Storage & Filesystem**: IDE/ATA disk driver, Virtual Filesystem (VFS) abstraction, FAT32/Ext2 reader, file descriptors (`open`, `read`, `write`, `close`).
- [ ] **Phase 6 — Userspace**: Ring 3 privilege transition, system call interface (`syscall`/`sysret`), ELF executable loader, freestanding C runtime (`libc`), interactive shell & core utilities.
- [ ] **Phase 7 — Graphics & Desktop**: Linear framebuffer compositor, 2D graphics primitives, PS/2 mouse driver, floating window manager, desktop environment.

---

## 11. Development Strategy

Every major subsystem follows the same progression:

```
┌──────────────┐
│ Understand   │
│ the hardware │
└──────┬───────┘
       │
       ▼
┌──────────────┐
│ Minimal      │
│implementation│
└──────┬───────┘
       │
       ▼
┌──────────────┐
│ Test in QEMU │
└──────┬───────┘
       │
       ▼
┌──────────────┐
│ Debug through│
│ serial / GDB │
└──────┬───────┘
       │
       ▼
┌──────────────┐
│ Formalize API│
│& abstraction │
└──────┬───────┘
       │
       ▼
┌──────────────┐
│ Integrate    │
│ with kernel  │
└──────────────┘
```

The project favors working vertical slices over disconnected components. For example, the scheduler is proven by:
$$\text{Timer Interrupt} \longrightarrow \text{Scheduler Invoked} \longrightarrow \text{Thread Selected} \longrightarrow \text{Context Switched} \longrightarrow \text{Thread Executes}$$

---

## 12. Debugging Philosophy

Kernel development fails differently from application development: invalid operations can cause triple faults, silent CPU resets, or corrupted memory tables.

Vailism maintains dual independent debugging paths:

```
                    Kernel
                   /      \
                  /        \
                 ▼          ▼
          Framebuffer      Serial (COM1)
              │                 │
         Visual Display    Machine Logs
                                │
                                ▼
                               GDB
```

- **Framebuffer**: For visual presentation, interactive shell, and terminal output.
- **Serial (COM1)**: Rock-solid stream of telemetry and kernel crash traces.
- **QEMU + GDB**: For instruction-level single-stepping and memory inspection.

---

## 13. What Vailism Is Not

Vailism is deliberately **not**:
- A Linux distribution or modified Linux kernel
- A collection of userland scripts
- A desktop theme or terminal wrapper
- A toy demonstration that stops at "Hello World"

The project is dedicated to engineering a complete, self-contained operating system from bare silicon.

---

## 14. Current Status

**Current Milestone:** Phase 4 Complete $\rightarrow$ Advancing to Phase 5 (Storage & Filesystem).

The system currently runs:
$$\text{Firmware} \rightarrow \text{Limine} \rightarrow \text{64-bit Kernel} \rightarrow \text{GDT/TSS} \rightarrow \text{IDT/ISRs} \rightarrow \text{PIT/Keyboard} \rightarrow \text{PMM/VMM/Heap} \rightarrow \text{Preemptive Multitasking Scheduler}$$

---

## 15. Long-Term Objective

The ultimate goal is to understand and control every stage of execution:

```
POWER
  │
  ▼
FIRMWARE
  │
  ▼
BOOTLOADER
  │
  ▼
KERNEL ENTRY
  ├── CPU (GDT / IDT)
  ├── MEMORY (PMM / VMM / Heap)
  ├── INTERRUPTS (PIC / APIC / Exceptions)
  ├── DEVICES (Serial / Framebuffer / Keyboard / Timer)
  ├── STORAGE (ATA / VFS)
  └── SCHEDULER (Processes / Threads)
          │
          ▼
       SYSCALL (Ring 3 Transition)
          │
          ▼
       USERLAND (libc / Shell / Utilities)
          │
          ▼
      APPLICATION (Window Manager / GUI)
```

---

## 📄 License

Vailism OS is licensed under the [MIT License](LICENSE).
