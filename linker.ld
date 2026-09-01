/*
 * Vailism OS Linker Script (x86_64 ELF)
 *
 * This script tells the linker (ld.lld) how to organize our kernel binary:
 * 1. Output format: 64-bit ELF for x86_64.
 * 2. Virtual base address: 0xffffffff80000000 (standard higher-half kernel mapping).
 * 3. Page alignment: 4096 bytes (4 KiB) for each segment.
 */

OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)

ENTRY(kmain)

PHDRS
{
    text    PT_LOAD    FLAGS((1 << 0) | (1 << 2)) ; /* Execute + Read */
    rodata  PT_LOAD    FLAGS((1 << 2)) ;            /* Read-Only */
    data    PT_LOAD    FLAGS((1 << 1) | (1 << 2)) ; /* Write + Read */
}

SECTIONS
{
    /* Higher-half virtual base address */
    . = 0xffffffff80000000;

    /* Code section */
    .text : {
        *(.text .text.*)
    } :text

    . = ALIGN(4096);

    /* Read-only data section */
    .rodata : {
        *(.rodata .rodata.*)
    } :rodata

    . = ALIGN(4096);

    /* Limine request tags placed in data segment */
    .requests : {
        KEEP(*(.requests_start_marker))
        KEEP(*(.requests))
        KEEP(*(.requests_end_marker))
    } :data

    . = ALIGN(4096);

    /* Initialized writable data */
    .data : {
        *(.data .data.*)
    } :data

    . = ALIGN(4096);

    /* Zero-initialized BSS section */
    .bss : {
        *(COMMON)
        *(.bss .bss.*)
    } :data

    /* Discard unnecessary host/compiler metadata */
    /DISCARD/ : {
        *(.eh_frame)
        *(.note .note.*)
        *(.comment)
    }
}
