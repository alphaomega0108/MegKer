; arch/x86_64/boot/usermode.asm
;
; Ring 3 entry. Two pieces:
;
;   enter_usermode(entry, user_stack, user_data_sel, user_code_sel)
;     Builds an iretq frame by hand and jumps to ring 3. Never
;     returns — the only way back to ring 0 is a fresh interrupt
;     (the syscall vector, in practice), which lands on the TSS's
;     RSP0 stack, not here.
;
;   process_trampoline
;     Reached via `ret` from arch_context_switch() (see
;     context_switch.asm), exactly like a new kernel thread's
;     trampoline — except this one's job is just to unpack a
;     launch_params_t* (stashed in r12 by process.c's fake initial
;     stack) into arguments and call enter_usermode with it.

[BITS 64]
global enter_usermode

enter_usermode:
    ; rdi=entry, rsi=user_stack, rdx=user_data_sel, rcx=user_code_sel
    mov ax, dx
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax          ; ss is loaded from the iretq frame below

    push rdx            ; SS
    push rsi            ; RSP
    pushfq
    pop  rax
    or   rax, 0x200      ; force IF=1 — ring 3 must run with interrupts enabled
    push rax            ; RFLAGS
    push rcx            ; CS
    push rdi            ; RIP
    iretq

global process_trampoline

process_trampoline:
    ; r12 = launch_params_t* { u64 entry, user_stack, data_sel, code_sel; }
    mov rdi, [r12 + 0]
    mov rsi, [r12 + 8]
    mov rdx, [r12 + 16]
    mov rcx, [r12 + 24]
    call enter_usermode
    ; enter_usermode never returns
.hang:
    cli
    hlt
    jmp .hang
