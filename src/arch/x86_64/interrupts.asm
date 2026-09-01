; ==============================================================================
; Vailism OS - Interrupt Service Routine (ISR) Stubs (x86_64 Assembly)
; ==============================================================================

section .text
extern isr_handler_dispatch

; Macro for exceptions that DO NOT push an error code (we push dummy 0)
%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push qword 0        ; Push dummy error code
    push qword %1       ; Push interrupt vector number
    jmp isr_common_stub
%endmacro

; Macro for exceptions that DO push an error code automatically by CPU
%macro ISR_ERRCODE 1
global isr%1
isr%1:
    push qword %1       ; Push interrupt vector number (error code already on stack)
    jmp isr_common_stub
%endmacro

; Macro for Hardware IRQs (32..47)
%macro IRQ_HANDLER 2
global irq%1
irq%1:
    push qword 0        ; Push dummy error code
    push qword %2       ; Push interrupt vector number (32 + %1)
    jmp isr_common_stub
%endmacro

; ------------------------------------------------------------------------------
; CPU Exception Stubs (0-31)
; ------------------------------------------------------------------------------
ISR_NOERRCODE 0   ; 0: Divide by Zero Exception (#DE)
ISR_NOERRCODE 1   ; 1: Debug Exception (#DB)
ISR_NOERRCODE 2   ; 2: Non-Maskable Interrupt (#NMI)
ISR_NOERRCODE 3   ; 3: Breakpoint Exception (#BP)
ISR_NOERRCODE 4   ; 4: Overflow Exception (#OF)
ISR_NOERRCODE 5   ; 5: Bound Range Exceeded (#BR)
ISR_NOERRCODE 6   ; 6: Invalid Opcode Exception (#UD)
ISR_NOERRCODE 7   ; 7: Device Not Available Exception (#NM)
ISR_ERRCODE   8   ; 8: Double Fault Exception (#DF) - has error code
ISR_NOERRCODE 9   ; 9: Coprocessor Segment Overrun
ISR_ERRCODE   10  ; 10: Invalid TSS Exception (#TS) - has error code
ISR_ERRCODE   11  ; 11: Segment Not Present (#NP) - has error code
ISR_ERRCODE   12  ; 12: Stack Fault Exception (#SS) - has error code
ISR_ERRCODE   13  ; 13: General Protection Fault (#GP) - has error code
ISR_ERRCODE   14  ; 14: Page Fault Exception (#PF) - has error code
ISR_NOERRCODE 15  ; 15: Reserved
ISR_NOERRCODE 16  ; 16: x87 FPU Floating-Point Error (#MF)
ISR_ERRCODE   17  ; 17: Alignment Check Exception (#AC) - has error code
ISR_NOERRCODE 18  ; 18: Machine Check Exception (#MC)
ISR_NOERRCODE 19  ; 19: SIMD Floating-Point Exception (#XM/#XF)
ISR_NOERRCODE 20  ; 20: Virtualization Exception (#VE)
ISR_ERRCODE   21  ; 21: Control Protection Exception (#CP) - has error code
ISR_NOERRCODE 22  ; 22: Reserved
ISR_NOERRCODE 23  ; 23: Reserved
ISR_NOERRCODE 24  ; 24: Reserved
ISR_NOERRCODE 25  ; 25: Reserved
ISR_NOERRCODE 26  ; 26: Reserved
ISR_NOERRCODE 27  ; 27: Reserved
ISR_NOERRCODE 28  ; 28: Hypervisor Injection Exception (#HV)
ISR_NOERRCODE 29  ; 29: VMM Communication Exception (#VC)
ISR_ERRCODE   30  ; 30: Security Exception (#SX) - has error code
ISR_NOERRCODE 31  ; 31: Reserved

; ------------------------------------------------------------------------------
; Hardware IRQ Stubs (32-47: Master PIC IRQ 0-7, Slave PIC IRQ 8-15)
; ------------------------------------------------------------------------------
IRQ_HANDLER 0,  32  ; IRQ0: Programmable Interval Timer (PIT)
IRQ_HANDLER 1,  33  ; IRQ1: PS/2 Keyboard
IRQ_HANDLER 2,  34  ; IRQ2: Cascade for Slave PIC
IRQ_HANDLER 3,  35  ; IRQ3: Serial COM2
IRQ_HANDLER 4,  36  ; IRQ4: Serial COM1
IRQ_HANDLER 5,  37  ; IRQ5: LPT2 / Sound Card
IRQ_HANDLER 6,  38  ; IRQ6: Floppy Disk
IRQ_HANDLER 7,  39  ; IRQ7: LPT1 / Spurious
IRQ_HANDLER 8,  40  ; IRQ8: Real Time Clock (RTC)
IRQ_HANDLER 9,  41  ; IRQ9: ACPI
IRQ_HANDLER 10, 42  ; IRQ10: Reserved / PCI
IRQ_HANDLER 11, 43  ; IRQ11: Reserved / PCI
IRQ_HANDLER 12, 44  ; IRQ12: PS/2 Mouse
IRQ_HANDLER 13, 45  ; IRQ13: FPU / Coprocessor
IRQ_HANDLER 14, 46  ; IRQ14: Primary ATA Hard Disk
IRQ_HANDLER 15, 47  ; IRQ15: Secondary ATA Hard Disk

; ------------------------------------------------------------------------------
; Common Interrupt Dispatcher Stub
; ------------------------------------------------------------------------------
isr_common_stub:
    ; 1. Save all general-purpose registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; 2. Pass pointer to the saved registers struct in RDI (1st param per ABI)
    mov rdi, rsp

    ; 3. Call the C dispatcher
    call isr_handler_dispatch

    ; 4. Restore all general-purpose registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; 5. Pop interrupt number and error code off the stack
    add rsp, 16

    ; 6. Return from interrupt (restores RIP, CS, RFLAGS, RSP, SS)
    iretq

; Function: void idt_flush(uint64_t idt_ptr_address)
global idt_flush
idt_flush:
    lidt [rdi]
    ret
