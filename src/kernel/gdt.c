#include "../include/gdt.h"
#include "../include/serial.h"

// External assembly flush routine
extern void gdt_flush(uint64_t gdt_ptr_address, uint16_t tss_selector);

// Dedicated double fault stack
static uint8_t double_fault_stack[4096];

// The GDT array: 5 standard 8-byte entries + 1 16-byte TSS descriptor (spans 2 entries) = 7 entries
static struct {
    struct gdt_entry entries[5];
    struct tss_descriptor tss;
} __attribute__((packed)) g_gdt;

static struct gdt_pointer g_gdt_ptr;
static struct tss_entry g_tss;

static void set_gdt_gate(int index, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    g_gdt.entries[index].base_low    = (uint16_t)(base & 0xFFFF);
    g_gdt.entries[index].base_middle = (uint8_t)((base >> 16) & 0xFF);
    g_gdt.entries[index].base_high   = (uint8_t)((base >> 24) & 0xFF);

    g_gdt.entries[index].limit_low   = (uint16_t)(limit & 0xFFFF);
    g_gdt.entries[index].granularity = (uint8_t)((limit >> 16) & 0x0F);
    g_gdt.entries[index].granularity |= (gran & 0xF0);
    g_gdt.entries[index].access      = access;
}

static void set_tss_descriptor(uint64_t tss_base, uint32_t tss_limit) {
    g_gdt.tss.length       = (uint16_t)(tss_limit & 0xFFFF);
    g_gdt.tss.base_low     = (uint16_t)(tss_base & 0xFFFF);
    g_gdt.tss.base_middle  = (uint8_t)((tss_base >> 16) & 0xFF);
    g_gdt.tss.flags1       = 0x89; // Present, Ring 0, 64-bit Available TSS
    g_gdt.tss.flags2       = (uint8_t)((tss_limit >> 16) & 0x0F);
    g_gdt.tss.base_high    = (uint8_t)((tss_base >> 24) & 0xFF);
    g_gdt.tss.base_highest = (uint32_t)((tss_base >> 32) & 0xFFFFFFFF);
    g_gdt.tss.reserved     = 0;
}

void gdt_init(void) {
    // 1. Entry 0x00: Null Descriptor
    set_gdt_gate(0, 0, 0, 0, 0);

    // 2. Entry 0x08: Kernel 64-bit Code Segment (Access 0x9A = Present, Ring 0, Executable, Readable; Gran 0x20 = 64-bit Long Mode)
    set_gdt_gate(1, 0, 0xFFFFF, 0x9A, 0x20);

    // 3. Entry 0x10: Kernel 64-bit Data Segment (Access 0x92 = Present, Ring 0, Writable)
    set_gdt_gate(2, 0, 0xFFFFF, 0x92, 0x00);

    // 4. Entry 0x18: User 64-bit Data Segment (Access 0xF2 = Present, Ring 3, Writable)
    set_gdt_gate(3, 0, 0xFFFFF, 0xF2, 0x00);

    // 5. Entry 0x20: User 64-bit Code Segment (Access 0xFA = Present, Ring 3, Executable, Readable; Gran 0x20 = 64-bit Long Mode)
    set_gdt_gate(4, 0, 0xFFFFF, 0xFA, 0x20);

    // 6. Zero out TSS structure
    for (size_t i = 0; i < sizeof(struct tss_entry); i++) {
        ((uint8_t *)&g_tss)[i] = 0;
    }

    // Set up IST1 (Interrupt Stack Table 1) for Double Fault exception recovery
    g_tss.ist1 = (uint64_t)double_fault_stack + sizeof(double_fault_stack);
    g_tss.iomap_base = sizeof(struct tss_entry);

    // 7. Entry 0x28: 16-byte 64-bit TSS Descriptor
    set_tss_descriptor((uint64_t)&g_tss, sizeof(struct tss_entry) - 1);

    // 8. Configure GDTR Pointer
    g_gdt_ptr.limit = sizeof(g_gdt) - 1;
    g_gdt_ptr.base  = (uint64_t)&g_gdt;

    // 9. Load GDT and flush registers
    gdt_flush((uint64_t)&g_gdt_ptr, GDT_TSS_SEGMENT);

    serial_puts("[GDT] 64-bit GDT & TSS initialized and loaded successfully.\n");
}
