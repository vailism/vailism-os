#include "../include/syscall.h"
#include "../include/vfs.h"
#include "../include/scheduler.h"
#include "../include/serial.h"
#include "../include/framebuffer.h"
#include "../include/io.h"

extern uint64_t msr_read(uint32_t msr);
extern void msr_write(uint32_t msr, uint64_t value);
extern void syscall_entry_stub(void);

void syscall_init(void) {
    // 1. Enable System Call Extension (SCE) in IA32_EFER
    uint64_t efer = msr_read(MSR_EFER);
    msr_write(MSR_EFER, efer | EFER_SCE);

    // 2. Configure Segment Selectors in IA32_STAR
    // Bits 48..63: User segment base (0x18)
    // Bits 32..47: Kernel segment base (0x08)
    uint64_t star = ((uint64_t)(0x18 | 3) << 48) | ((uint64_t)0x08 << 32);
    msr_write(MSR_STAR, star);

    // 3. Set LSTAR to our assembly entry handler
    msr_write(MSR_LSTAR, (uint64_t)syscall_entry_stub);

    // 4. Set FMASK to clear Interrupt Flag (0x200) upon syscall entry
    msr_write(MSR_FMASK, 0x200);

    serial_puts("[SYSCALL] Fast x86_64 syscall/sysret MSRs configured successfully.\n");
}

int64_t syscall_dispatch(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    (void)arg4;
    (void)arg5;

    switch (num) {
        case SYS_READ:
            return vfs_read((int)arg1, (void *)arg2, (size_t)arg3);

        case SYS_WRITE:
            // Standard Output (1) or Standard Error (2)
            if (arg1 == 1 || arg1 == 2) {
                const char *buf = (const char *)arg2;
                size_t len = (size_t)arg3;
                for (size_t i = 0; i < len; i++) {
                    fb_putchar(buf[i]);
                    if (buf[i] == '\n') serial_putchar('\r');
                    serial_putchar(buf[i]);
                }
                return (int64_t)len;
            }
            return vfs_write((int)arg1, (const void *)arg2, (size_t)arg3);

        case SYS_OPEN:
            return vfs_open((const char *)arg1, (int)arg2);

        case SYS_CLOSE:
            return vfs_close((int)arg1);

        case SYS_YIELD:
            yield();
            return 0;

        case SYS_SLEEP:
            thread_sleep(arg1);
            return 0;

        case SYS_GETPID: {
            thread_t *curr = get_current_thread();
            return curr ? (int64_t)curr->tid : 0;
        }

        case SYS_EXIT:
            thread_exit();
            return 0;

        case SYS_REBOOT:
            serial_puts("[REBOOT] Hardware reboot requested via syscall.\n");
            // Pulse the CPU reset line via 8042 keyboard controller
            outb(0x64, 0xFE);
            for (;;) { __asm__ volatile ("hlt"); }

        default:
            serial_puts("[SYSCALL] Unknown syscall vector called!\n");
            return -1;
    }
}
