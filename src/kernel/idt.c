#include "../include/idt.h"
#include "../include/pic.h"
#include "../include/serial.h"
#include "../include/framebuffer.h"

// External assembly symbols for ISR stubs
extern void isr0(void);  extern void isr1(void);  extern void isr2(void);  extern void isr3(void);
extern void isr4(void);  extern void isr5(void);  extern void isr6(void);  extern void isr7(void);
extern void isr8(void);  extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void); extern void isr15(void);
extern void isr16(void); extern void isr17(void); extern void isr18(void); extern void isr19(void);
extern void isr20(void); extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void); extern void isr27(void);
extern void isr28(void); extern void isr29(void); extern void isr30(void); extern void isr31(void);

// External assembly symbols for IRQ stubs
extern void irq0(void);  extern void irq1(void);  extern void irq2(void);  extern void irq3(void);
extern void irq4(void);  extern void irq5(void);  extern void irq6(void);  extern void irq7(void);
extern void irq8(void);  extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void); extern void irq15(void);

extern void idt_flush(uint64_t idt_ptr_address);

static struct idt_entry g_idt[256];
static struct idt_pointer g_idt_ptr;
static isr_handler_t g_interrupt_handlers[256];

static const char *exception_messages[32] = {
    "Divide by Zero (#DE)",
    "Debug Exception (#DB)",
    "Non-Maskable Interrupt (#NMI)",
    "Breakpoint (#BP)",
    "Overflow (#OF)",
    "Bound Range Exceeded (#BR)",
    "Invalid Opcode (#UD)",
    "Device Not Available (#NM)",
    "Double Fault (#DF)",
    "Coprocessor Segment Overrun",
    "Invalid TSS (#TS)",
    "Segment Not Present (#NP)",
    "Stack-Segment Fault (#SS)",
    "General Protection Fault (#GP)",
    "Page Fault (#PF)",
    "Reserved",
    "x87 Floating-Point Exception (#MF)",
    "Alignment Check (#AC)",
    "Machine Check (#MC)",
    "SIMD Floating-Point Exception (#XM)",
    "Virtualization Exception (#VE)",
    "Control Protection Exception (#CP)",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Hypervisor Injection Exception (#HV)",
    "VMM Communication Exception (#VC)",
    "Security Exception (#SX)",
    "Reserved"
};

void idt_set_gate(uint8_t vector, uint64_t handler_addr, uint16_t selector, uint8_t flags, uint8_t ist) {
    g_idt[vector].offset_low      = (uint16_t)(handler_addr & 0xFFFF);
    g_idt[vector].selector        = selector;
    g_idt[vector].ist             = ist & 0x07;
    g_idt[vector].type_attributes = flags;
    g_idt[vector].offset_middle   = (uint16_t)((handler_addr >> 16) & 0xFFFF);
    g_idt[vector].offset_high     = (uint32_t)((handler_addr >> 32) & 0xFFFFFFFF);
    g_idt[vector].reserved        = 0;
}

void register_interrupt_handler(uint8_t vector, isr_handler_t handler) {
    g_interrupt_handlers[vector] = handler;
}

static void print_hex(uint64_t val) {
    char buf[19];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 15; i >= 0; i--) {
        uint8_t nibble = (val >> (i * 4)) & 0x0F;
        buf[17 - i] = (nibble < 10) ? ('0' + nibble) : ('A' + (nibble - 10));
    }
    buf[18] = '\0';
    serial_puts(buf);
    fb_puts(buf);
}

void isr_handler_dispatch(struct registers *regs) {
    // Check if custom handler is registered
    if (g_interrupt_handlers[regs->int_num] != NULL) {
        g_interrupt_handlers[regs->int_num](regs);
    } else if (regs->int_num < 32) {
        // Unhandled CPU Exception -> Kernel Panic
        fb_set_color(FB_COLOR_RED, FB_COLOR_BG);
        serial_puts("\n================ KERNEL PANIC: CPU EXCEPTION ================\n");
        fb_puts("\n================ KERNEL PANIC: CPU EXCEPTION ================\n");

        serial_puts("Exception: ");
        serial_puts(exception_messages[regs->int_num]);
        serial_puts("\nVector: ");
        print_hex(regs->int_num);
        serial_puts("  Error Code: ");
        print_hex(regs->error_code);
        serial_puts("\nRIP: ");
        print_hex(regs->rip);
        serial_puts("  RSP: ");
        print_hex(regs->rsp);
        serial_puts("  RFLAGS: ");
        print_hex(regs->rflags);
        serial_puts("\n");

        fb_puts("Exception: ");
        fb_puts(exception_messages[regs->int_num]);
        fb_puts("\nVector: ");
        print_hex(regs->int_num);
        fb_puts("  Error Code: ");
        print_hex(regs->error_code);
        fb_puts("\nRIP: ");
        print_hex(regs->rip);
        fb_puts("  RSP: ");
        print_hex(regs->rsp);
        fb_puts("  RFLAGS: ");
        print_hex(regs->rflags);
        fb_puts("\nSystem Halted.\n");

        // Halt machine
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }

    // If it was a hardware IRQ (32..47), send EOI to PIC
    if (regs->int_num >= 32 && regs->int_num < 48) {
        pic_send_eoi((uint8_t)(regs->int_num - 32));
    }
}

void idt_init(void) {
    // Clear IDT table and handlers
    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, 0, 0, 0, 0);
        g_interrupt_handlers[i] = NULL;
    }

    // Set up CPU Exception gates (0-31) with Ring 0 Interrupt Gate (0x8E)
    // Note: Double Fault (vector 8) is mapped to IST1 for stack safety!
    idt_set_gate(0,  (uint64_t)isr0,  0x08, 0x8E, 0);
    idt_set_gate(1,  (uint64_t)isr1,  0x08, 0x8E, 0);
    idt_set_gate(2,  (uint64_t)isr2,  0x08, 0x8E, 0);
    idt_set_gate(3,  (uint64_t)isr3,  0x08, 0x8E, 0);
    idt_set_gate(4,  (uint64_t)isr4,  0x08, 0x8E, 0);
    idt_set_gate(5,  (uint64_t)isr5,  0x08, 0x8E, 0);
    idt_set_gate(6,  (uint64_t)isr6,  0x08, 0x8E, 0);
    idt_set_gate(7,  (uint64_t)isr7,  0x08, 0x8E, 0);
    idt_set_gate(8,  (uint64_t)isr8,  0x08, 0x8E, 1); // Uses IST1 stack
    idt_set_gate(9,  (uint64_t)isr9,  0x08, 0x8E, 0);
    idt_set_gate(10, (uint64_t)isr10, 0x08, 0x8E, 0);
    idt_set_gate(11, (uint64_t)isr11, 0x08, 0x8E, 0);
    idt_set_gate(12, (uint64_t)isr12, 0x08, 0x8E, 0);
    idt_set_gate(13, (uint64_t)isr13, 0x08, 0x8E, 0);
    idt_set_gate(14, (uint64_t)isr14, 0x08, 0x8E, 0);
    idt_set_gate(15, (uint64_t)isr15, 0x08, 0x8E, 0);
    idt_set_gate(16, (uint64_t)isr16, 0x08, 0x8E, 0);
    idt_set_gate(17, (uint64_t)isr17, 0x08, 0x8E, 0);
    idt_set_gate(18, (uint64_t)isr18, 0x08, 0x8E, 0);
    idt_set_gate(19, (uint64_t)isr19, 0x08, 0x8E, 0);
    idt_set_gate(20, (uint64_t)isr20, 0x08, 0x8E, 0);
    idt_set_gate(21, (uint64_t)isr21, 0x08, 0x8E, 0);
    idt_set_gate(22, (uint64_t)isr22, 0x08, 0x8E, 0);
    idt_set_gate(23, (uint64_t)isr23, 0x08, 0x8E, 0);
    idt_set_gate(24, (uint64_t)isr24, 0x08, 0x8E, 0);
    idt_set_gate(25, (uint64_t)isr25, 0x08, 0x8E, 0);
    idt_set_gate(26, (uint64_t)isr26, 0x08, 0x8E, 0);
    idt_set_gate(27, (uint64_t)isr27, 0x08, 0x8E, 0);
    idt_set_gate(28, (uint64_t)isr28, 0x08, 0x8E, 0);
    idt_set_gate(29, (uint64_t)isr29, 0x08, 0x8E, 0);
    idt_set_gate(30, (uint64_t)isr30, 0x08, 0x8E, 0);
    idt_set_gate(31, (uint64_t)isr31, 0x08, 0x8E, 0);

    // Set up Hardware IRQ gates (32-47)
    idt_set_gate(32, (uint64_t)irq0,  0x08, 0x8E, 0);
    idt_set_gate(33, (uint64_t)irq1,  0x08, 0x8E, 0);
    idt_set_gate(34, (uint64_t)irq2,  0x08, 0x8E, 0);
    idt_set_gate(35, (uint64_t)irq3,  0x08, 0x8E, 0);
    idt_set_gate(36, (uint64_t)irq4,  0x08, 0x8E, 0);
    idt_set_gate(37, (uint64_t)irq5,  0x08, 0x8E, 0);
    idt_set_gate(38, (uint64_t)irq6,  0x08, 0x8E, 0);
    idt_set_gate(39, (uint64_t)irq7,  0x08, 0x8E, 0);
    idt_set_gate(40, (uint64_t)irq8,  0x08, 0x8E, 0);
    idt_set_gate(41, (uint64_t)irq9,  0x08, 0x8E, 0);
    idt_set_gate(42, (uint64_t)irq10, 0x08, 0x8E, 0);
    idt_set_gate(43, (uint64_t)irq11, 0x08, 0x8E, 0);
    idt_set_gate(44, (uint64_t)irq12, 0x08, 0x8E, 0);
    idt_set_gate(45, (uint64_t)irq13, 0x08, 0x8E, 0);
    idt_set_gate(46, (uint64_t)irq14, 0x08, 0x8E, 0);
    idt_set_gate(47, (uint64_t)irq15, 0x08, 0x8E, 0);

    // Configure IDTR pointer
    g_idt_ptr.limit = sizeof(g_idt) - 1;
    g_idt_ptr.base  = (uint64_t)&g_idt;

    // Load IDTR with lidt instruction
    idt_flush((uint64_t)&g_idt_ptr);

    serial_puts("[IDT] 256-vector IDT initialized and loaded into CPU.\n");
}
