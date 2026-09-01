#ifndef GDT_H
#define GDT_H

#include "types.h"

#define GDT_KERNEL_CODE_SEGMENT 0x08
#define GDT_KERNEL_DATA_SEGMENT 0x10
#define GDT_USER_DATA_SEGMENT   0x1B // 0x18 | 3 (RPL 3)
#define GDT_USER_CODE_SEGMENT   0x23 // 0x20 | 3 (RPL 3)
#define GDT_TSS_SEGMENT         0x28

/**
 * 64-bit Task State Segment (TSS) structure.
 * In x86_64, the TSS holds the kernel stack pointer (RSP0) used during
 * privilege level transitions (Ring 3 -> Ring 0) and the Interrupt Stack Table (IST1-IST7).
 */
struct tss_entry {
    uint32_t reserved0;
    uint64_t rsp0;       // Stack pointer loaded on privilege transition to Ring 0
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;       // Interrupt Stack Table 1 (e.g. for Double Faults)
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base; // I/O Permission Bitmap Base Address
} __attribute__((packed));

/**
 * Standard 8-byte GDT Entry
 */
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

/**
 * In x86_64, a TSS descriptor spans 16 bytes (two standard GDT entries).
 */
struct tss_descriptor {
    uint16_t length;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  flags1;
    uint8_t  flags2;
    uint8_t  base_high;
    uint32_t base_highest;
    uint32_t reserved;
} __attribute__((packed));

/**
 * GDTR pointer structure passed to the 'lgdt' CPU instruction.
 */
struct gdt_pointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

/**
 * Initialize the GDT and TSS, then flush and reload CPU segment registers.
 */
void gdt_init(void);

#endif // GDT_H
