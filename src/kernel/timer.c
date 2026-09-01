#include "../include/timer.h"
#include "../include/idt.h"
#include "../include/io.h"
#include "../include/serial.h"

static volatile uint64_t g_timer_ticks = 0;
static uint32_t g_timer_freq = 100;

static void timer_callback(struct registers *regs) {
    (void)regs;
    g_timer_ticks++;
}

void timer_init(uint32_t frequency_hz) {
    g_timer_freq = frequency_hz;
    uint32_t divisor = PIT_BASE_FREQUENCY / frequency_hz;

    // Register callback on IRQ0 (Vector 32)
    register_interrupt_handler(32, timer_callback);

    // Command byte 0x36 = Channel 0, Lobyle/Hibyte, Mode 3 (Square Wave), Binary
    outb(PIT_COMMAND_PORT, 0x36);

    // Send divisor low byte then high byte
    uint8_t low  = (uint8_t)(divisor & 0xFF);
    uint8_t high = (uint8_t)((divisor >> 8) & 0xFF);
    outb(PIT_CHANNEL0_DATA, low);
    outb(PIT_CHANNEL0_DATA, high);

    serial_puts("[PIT] Programmable Interval Timer configured to 100 Hz.\n");
}

uint64_t timer_get_ticks(void) {
    return g_timer_ticks;
}

void timer_sleep(uint64_t ms) {
    uint64_t target_ticks = g_timer_ticks + (ms * g_timer_freq / 1000);
    while (g_timer_ticks < target_ticks) {
        __asm__ volatile ("hlt");
    }
}
