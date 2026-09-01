; ==============================================================================
; Vailism OS - Context Switching & Thread Bootstrap (x86_64 Assembly)
; ==============================================================================

section .text
global context_switch
global thread_entry_trampoline
extern thread_exit

; Function: void context_switch(uint64_t *prev_rsp_ptr, uint64_t next_rsp)
; Parameters (per System V AMD64 ABI):
;   RDI = Pointer to prev->rsp (where old stack pointer is stored)
;   RSI = Value of next->rsp (the new stack pointer to load)
context_switch:
    ; 1. Save callee-preserved registers of the current thread onto its stack
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    ; 2. Save current RSP into prev->rsp
    mov [rdi], rsp

    ; 3. Load new RSP from next_rsp
    mov rsp, rsi

    ; 4. Restore callee-preserved registers from the new thread's stack
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx

    ; 5. Return into the new thread (pops RIP and jumps!)
    ret

; Function: thread_entry_trampoline
; Invoked when a newly created thread is scheduled for the first time.
; Registers set up by thread_create:
;   R12 = entry function pointer
;   R13 = argument pointer (arg)
thread_entry_trampoline:
    ; Ensure interrupts are enabled for the new thread
    sti

    ; Set up first argument per ABI (RDI = arg)
    mov rdi, r13

    ; Call thread entry point
    call r12

    ; If thread function returns, cleanly terminate thread
    call thread_exit

    ; Safety halt if thread_exit ever returns
.halt_loop:
    cli
    hlt
    jmp .halt_loop
