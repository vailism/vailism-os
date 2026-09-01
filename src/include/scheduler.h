#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "types.h"
#include "process.h"

#define DEFAULT_TIME_SLICE 5 // 5 ticks = 50ms time slice at 100 Hz

/**
 * Initialize the preemptive multitasking scheduler.
 * Converts the current kmain bootstrap context into Thread 0.
 */
void scheduler_init(void);

/**
 * Create a new kernel thread and add it to the ready queue.
 */
thread_t *thread_create(const char *name, thread_entry_t entry, void *arg);

/**
 * Invoke the scheduler to select the next ready thread and perform a context switch.
 */
void scheduler_schedule(void);

/**
 * Voluntarily yield the CPU to the next ready thread.
 */
void yield(void);

/**
 * Put the current thread to sleep for the specified duration (in milliseconds).
 */
void thread_sleep(uint64_t ms);

/**
 * Terminate the current thread and yield to another thread.
 */
void thread_exit(void);

/**
 * Called by the PIT timer interrupt on every 100 Hz tick to drive preemption and sleep wakeups.
 */
void scheduler_timer_tick(void);

/**
 * Get pointer to the currently running thread.
 */
thread_t *get_current_thread(void);

#endif // SCHEDULER_H
