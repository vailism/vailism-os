#ifndef PIC_H
#define PIC_H

#include "types.h"

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

#define PIC_EOI      0x20

#define IRQ_BASE     32 // IRQ 0..15 mapped to interrupt vectors 32..47

/**
 * Initialize and remap the 8259 Master & Slave PICs to vectors 32-47.
 */
void pic_init(void);

/**
 * Send End of Interrupt (EOI) signal to the PIC.
 */
void pic_send_eoi(uint8_t irq);

/**
 * Mask (disable) a specific IRQ line (0-15).
 */
void pic_mask_irq(uint8_t irq);

/**
 * Unmask (enable) a specific IRQ line (0-15).
 */
void pic_unmask_irq(uint8_t irq);

/**
 * Disable the legacy 8259 PIC entirely (used when transitioning to APIC).
 */
void pic_disable(void);

#endif // PIC_H
