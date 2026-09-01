; ==============================================================================
; Vailism OS - GDT Flush and Reload (x86_64 Assembly)
; ==============================================================================

section .text
global gdt_flush

; Function: void gdt_flush(uint64_t gdt_ptr_address, uint16_t tss_selector)
; Inputs:
;   RDI = Address of GDT pointer struct (passed to lgdt)
;   RSI = TSS Selector (e.g. 0x28)
gdt_flush:
    ; 1. Load the new Global Descriptor Table register
    lgdt [rdi]

    ; 2. Reload Code Segment (CS) to 0x08 using a 64-bit far return (retfq)
    push 0x08               ; Push 64-bit Kernel Code Segment selector
    lea rax, [rel .reload_cs] ; Push address of next instruction
    push rax
    retfq                   ; Far return: pops RIP then CS, cleanly updating CS!

.reload_cs:
    ; 3. Reload Data Segment registers with Kernel Data Segment (0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; 4. Load Task Register (LTR) with the TSS segment selector
    mov ax, si
    ltr ax

    ret
