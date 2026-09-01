#ifndef IO_H
#define IO_H

#include "types.h"

/**
 * outb: Output a single byte to an x86 I/O port.
 * 
 * Assembly explanation:
 *   "outb %0, %1"
 *   - %0 corresponds to 'val' (placed in 8-bit AL register, constraint "a")
 *   - %1 corresponds to 'port' (placed in 16-bit DX register, constraint "Nd")
 */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

/**
 * inb: Read a single byte from an x86 I/O port.
 * 
 * Assembly explanation:
 *   "inb %1, %0"
 *   - %0 is the output 'val' received from the port (into AL register, constraint "=a")
 *   - %1 is the input 'port' (in DX register, constraint "Nd")
 */
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/**
 * io_wait: Short pause for slow legacy hardware buses.
 * Writing to port 0x80 (unused post code port) takes ~1 microsecond.
 */
static inline void io_wait(void) {
    outb(0x80, 0);
}

#endif // IO_H
