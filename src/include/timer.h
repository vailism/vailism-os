#ifndef TIMER_H
#define TIMER_H

#include "types.h"

#define PIT_CHANNEL0_DATA 0x40
#define PIT_COMMAND_PORT  0x43
#define PIT_BASE_FREQUENCY 1193182

/**
 * Initialize the 8254 PIT timer to fire IRQ0 at the specified frequency (in Hz).
 */
void timer_init(uint32_t frequency_hz);

/**
 * Get total uptime ticks elapsed since boot.
 */
uint64_t timer_get_ticks(void);

/**
 * Sleep for a specified number of milliseconds (busy-wait via timer ticks).
 */
void timer_sleep(uint64_t ms);

#endif // TIMER_H
