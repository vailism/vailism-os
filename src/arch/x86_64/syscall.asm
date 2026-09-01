; ==============================================================================
; Vailism OS - System Call Assembly Entry & MSR Access (x86_64)
; ==============================================================================

section .text
global syscall_entry_stub
global msr_read
global msr_write
extern syscall_dispatch

; Function: uint64_t msr_read(uint32_t msr)
; Reads 64-bit Model Specific Register (ECX = msr_id) -> EDX:EAX
msr_read:
    mov ecx, edi
    rdmsr
    shl rdx, 32
    or rax, rdx
    ret

; Function: void msr_write(uint32_t msr, uint64_t value)
; Writes 64-bit value (in RSI) to MSR (in RDI) -> EDX:EAX
msr_write:
    mov ecx, edi
    mov eax, esi
    mov rdx, rsi
    shr rdx, 32
    wrmsr
    ret

; Entry point jumped to by CPU upon 'syscall' instruction
syscall_entry_stub:
    ; 1. Preserve user context and registers
    push rcx        ; User return RIP saved by CPU in RCX
    push r11        ; User RFLAGS saved by CPU in R11
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    ; 2. Shift registers for C calling convention:
    ; User syscall arguments: RAX=num, RDI=a1, RSI=a2, RDX=a3, R10=a4, R8=a5, R9=a6
    ; C function expects:     RDI=num, RSI=a1, RDX=a2, RCX=a3, R8=a4,  R9=a5
    push r9         ; a6
    push r8         ; a5
    mov r9, r10     ; a4
    mov r8, rdx     ; a3
    mov rcx, rsi    ; a2
    mov rdx, rdi    ; a1
    mov rdi, rax    ; syscall number

    ; 3. Call C dispatcher
    call syscall_dispatch

    ; Clean up stack arguments
    add rsp, 16

    ; 4. Restore preserved registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    pop r11         ; Restore user RFLAGS
    pop rcx         ; Restore user RIP

    ; 5. Return to caller (sysret restores user Ring 3 mode & jumps to RCX)
    o64 sysret
