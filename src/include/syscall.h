#ifndef SYSCALL_H
#define SYSCALL_H

#include "types.h"
#include "idt.h"

// Standard x86_64 Syscall Numbers
#define SYS_READ      0
#define SYS_WRITE     1
#define SYS_OPEN      2
#define SYS_CLOSE     3
#define SYS_YIELD     24
#define SYS_SLEEP     35
#define SYS_GETPID    39
#define SYS_EXIT      60
#define SYS_REBOOT    169

// Model Specific Registers (MSRs) for x86_64 fast syscall/sysret
#define MSR_EFER      0xC0000080
#define MSR_STAR      0xC0000081
#define MSR_LSTAR     0xC0000082
#define MSR_CSTAR     0xC0000083
#define MSR_FMASK     0xC0000084

#define EFER_SCE      0x01 // System Call Enable bit

/**
 * Initialize x86_64 fast syscall/sysret MSRs.
 */
void syscall_init(void);

/**
 * C System Call Dispatcher.
 */
int64_t syscall_dispatch(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5);

#endif // SYSCALL_H
