#include "../include/scheduler.h"
#include "../include/heap.h"
#include "../include/memory.h"
#include "../include/string.h"
#include "../include/timer.h"
#include "../include/serial.h"

extern void context_switch(uint64_t *prev_rsp_ptr, uint64_t next_rsp);
extern void thread_entry_trampoline(void);

static thread_t g_threads[MAX_THREADS];
static thread_t *g_current_thread = NULL;
static uint64_t g_next_tid = 1;
static bool g_scheduler_active = false;

void scheduler_init(void) {
    memset(g_threads, 0, sizeof(g_threads));

    // Initialize Thread 0 (The Main / Kernel bootstrap thread)
    g_threads[0].tid = 0;
    strncpy(g_threads[0].name, "kmain_idle", 31);
    g_threads[0].state = THREAD_RUNNING;
    g_threads[0].time_slice = DEFAULT_TIME_SLICE;
    g_threads[0].stack_base = NULL; // Uses current bootstrap stack
    g_threads[0].rsp = 0;

    g_current_thread = &g_threads[0];
    g_scheduler_active = true;

    serial_puts("[SCHEDULER] Preemptive Round-Robin Scheduler initialized (Thread 0 running).\n");
}

thread_t *thread_create(const char *name, thread_entry_t entry, void *arg) {
    // Find unused slot
    int slot = -1;
    for (int i = 1; i < MAX_THREADS; i++) {
        if (g_threads[i].state == THREAD_UNUSED || g_threads[i].state == THREAD_TERMINATED) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        serial_puts("[SCHEDULER ERROR] Maximum thread count reached!\n");
        return NULL;
    }

    thread_t *t = &g_threads[slot];
    memset(t, 0, sizeof(thread_t));

    t->tid = g_next_tid++;
    strncpy(t->name, name ? name : "unnamed_thread", 31);

    // Allocate thread kernel stack
    t->stack_base = kmalloc(KERNEL_STACK_SIZE);
    if (!t->stack_base) {
        serial_puts("[SCHEDULER ERROR] Failed to allocate stack for new thread!\n");
        return NULL;
    }

    // Top of stack (grows downwards)
    uint64_t stack_top = (uint64_t)t->stack_base + KERNEL_STACK_SIZE;
    stack_top = ALIGN_DOWN(stack_top, 16);

    // Prepare initial stack frame matching context_switch pop sequence
    stack_top -= sizeof(struct cpu_context);
    struct cpu_context *ctx = (struct cpu_context *)stack_top;

    ctx->r15 = 0;
    ctx->r14 = 0;
    ctx->r13 = (uint64_t)arg;                     // Passed to thread_entry_trampoline
    ctx->r12 = (uint64_t)entry;                   // Entry point function
    ctx->rbp = 0;
    ctx->rbx = 0;
    ctx->rip = (uint64_t)thread_entry_trampoline; // Initial return address

    t->rsp = stack_top;
    t->state = THREAD_READY;
    t->time_slice = DEFAULT_TIME_SLICE;

    serial_puts("[SCHEDULER] Created thread '");
    serial_puts(t->name);
    serial_puts("'\n");

    return t;
}

void scheduler_schedule(void) {
    if (!g_scheduler_active) return;

    // Wake up any sleeping threads whose target tick has arrived
    uint64_t current_ticks = timer_get_ticks();
    for (int i = 0; i < MAX_THREADS; i++) {
        if (g_threads[i].state == THREAD_SLEEPING && current_ticks >= g_threads[i].sleep_until_ticks) {
            g_threads[i].state = THREAD_READY;
        }
    }

    // Find next ready thread in round-robin order
    thread_t *prev = g_current_thread;
    thread_t *next = NULL;

    int start_index = (int)(prev - g_threads);
    for (int i = 1; i <= MAX_THREADS; i++) {
        int idx = (start_index + i) % MAX_THREADS;
        if (g_threads[idx].state == THREAD_READY) {
            next = &g_threads[idx];
            break;
        }
    }

    // If no other thread is ready, continue running current thread if it's still runnable
    if (!next) {
        if (prev->state == THREAD_RUNNING || prev->state == THREAD_READY) {
            prev->state = THREAD_RUNNING;
            prev->time_slice = DEFAULT_TIME_SLICE;
            return;
        }
        // If current thread is sleeping, wait for timer interrupt
        if (prev->state == THREAD_SLEEPING) {
            while (timer_get_ticks() < prev->sleep_until_ticks) {
                __asm__ volatile ("sti; hlt");
            }
            prev->state = THREAD_RUNNING;
            prev->time_slice = DEFAULT_TIME_SLICE;
            return;
        }
        next = &g_threads[0];
    }

    if (next == prev) {
        next->state = THREAD_RUNNING;
        next->time_slice = DEFAULT_TIME_SLICE;
        return;
    }

    // State transition
    if (prev->state == THREAD_RUNNING) {
        prev->state = THREAD_READY;
    }

    next->state = THREAD_RUNNING;
    next->time_slice = DEFAULT_TIME_SLICE;
    g_current_thread = next;

    // Perform assembly context switch
    context_switch(&prev->rsp, next->rsp);
}

void yield(void) {
    scheduler_schedule();
}

void thread_sleep(uint64_t ms) {
    if (!g_current_thread) return;

    // 100 Hz = 10 ms per tick
    uint64_t ticks_to_sleep = (ms * 100) / 1000;
    if (ticks_to_sleep == 0) ticks_to_sleep = 1;

    g_current_thread->sleep_until_ticks = timer_get_ticks() + ticks_to_sleep;
    g_current_thread->state = THREAD_SLEEPING;

    scheduler_schedule();
}

void thread_exit(void) {
    if (!g_current_thread) return;

    serial_puts("[SCHEDULER] Thread '");
    serial_puts(g_current_thread->name);
    serial_puts("' terminated.\n");

    g_current_thread->state = THREAD_TERMINATED;
    scheduler_schedule();

    // Should never reach here
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void scheduler_timer_tick(void) {
    if (!g_scheduler_active || !g_current_thread) return;

    // Check if current thread's time slice has expired
    if (g_current_thread->time_slice > 0) {
        g_current_thread->time_slice--;
    }

    if (g_current_thread->time_slice == 0) {
        scheduler_schedule();
    }
}

thread_t *get_current_thread(void) {
    return g_current_thread;
}
