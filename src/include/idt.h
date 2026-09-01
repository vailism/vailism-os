#ifndef IDT_H
#define IDT_H

#include "types.h"

/**
 * 64-bit IDT Gate Descriptor (16 bytes per entry in x86_64).
 */
struct idt_entry {
    uint16_t offset_low;        // Target handler bits 0..15
    uint16_t selector;          // Code Segment selector (0x08)
    uint8_t  ist;               // Interrupt Stack Table index (0 = none, 1-7 = IST1-IST7)
    uint8_t  type_attributes;   // Gate type, DPL, Present bit (0x8E = 64-bit Ring 0 Interrupt Gate)
    uint16_t offset_middle;     // Target handler bits 16..31
    uint32_t offset_high;       // Target handler bits 32..63
    uint32_t reserved;          // Reserved (must be 0)
} __attribute__((packed));

/**
 * IDTR pointer struct passed to the 'lidt' CPU instruction.
 */
struct idt_pointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

/**
 * Structure containing all CPU register states pushed by our assembly ISR stub.
 */
struct registers {
    // Pushed by 'push rax...push r15' in our assembly stub
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;

    // Interrupt Vector number and Error Code
    uint64_t int_num;
    uint64_t error_code;

    // Pushed automatically by CPU upon interrupt
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed));

typedef void (*isr_handler_t)(struct registers *regs);

/**
 * Initialize IDT table, register all 32 exception handlers, and load IDTR.
 */
void idt_init(void);

/**
 * Register a custom handler function for a specific interrupt vector (0-255).
 */
void register_interrupt_handler(uint8_t vector, isr_handler_t handler);

/**
 * Set an IDT gate descriptor.
 */
void idt_set_gate(uint8_t vector, uint64_t handler_addr, uint16_t selector, uint8_t flags, uint8_t ist);

#endif // IDT_H
