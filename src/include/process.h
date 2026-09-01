#ifndef PROCESS_H
#define PROCESS_H

#include "types.h"
#include "vmm.h"

#define KERNEL_STACK_SIZE 16384 // 16 KiB kernel stack per thread
#define MAX_THREADS       64

typedef enum {
    THREAD_UNUSED = 0,
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_SLEEPING,
    THREAD_TERMINATED
} thread_state_t;

typedef void (*thread_entry_t)(void *arg);

/**
 * CPU Context pushed onto the thread stack for context switching.
 */
struct cpu_context {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t rbp;
    uint64_t rbx;
    uint64_t rip; // Return instruction pointer (popped by 'ret')
} __attribute__((packed));

/**
 * Thread Control Block (TCB)
 */
typedef struct thread {
    uint64_t        tid;                // Thread ID
    char            name[32];           // Human-readable thread name
    thread_state_t  state;              // READY, RUNNING, SLEEPING, TERMINATED
    uint64_t        rsp;                // Saved Stack Pointer (top of stack)
    void           *stack_base;         // Base address of allocated stack buffer
    uint64_t        sleep_until_ticks;  // Target tick count if state == THREAD_SLEEPING
    uint32_t        time_slice;         // Remaining timer ticks in current quantum
    struct thread  *next;               // Linked-list pointer for queues
} thread_t;

/**
 * Process Control Block (PCB)
 */
typedef struct process {
    uint64_t        pid;
    char            name[32];
    pagetable_t    *pml4;               // Virtual address space
    thread_t       *main_thread;
} process_t;

#endif // PROCESS_H
