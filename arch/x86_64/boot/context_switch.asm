; arch/x86_64/boot/context_switch.asm
;
; arch_context_switch(void** old_sp, void* new_sp)
;   rdi = old_sp, rsi = new_sp
;
; Saves the callee-saved registers System V doesn't already guarantee
; get preserved across a call, stashes the resulting rsp into *old_sp,
; loads new_sp, and pops the same set back — which either resumes a
; previously-suspended thread exactly where its own call to this
; function left off, or (for a brand new thread) lands on values
; thread_create()/arch_thread_init_stack() deliberately arranged to
; look like that, `ret`-ing into thread_trampoline below instead.

[BITS 64]
global arch_context_switch

arch_context_switch:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    mov  [rdi], rsp      ; *old_sp = rsp
    mov  rsp, rsi         ; rsp = new_sp

    pop  r15
    pop  r14
    pop  r13
    pop  r12
    pop  rbx
    pop  rbp
    ret

; Entry point for a thread that's never run before. Reached via the
; `ret` above, not a call — so it inherits whatever thread_create()
; put in r12/r13 as if they'd been "restored" like any other register.
global thread_trampoline
extern thread_exit

thread_trampoline:
    sti                  ; new threads must not inherit IF=0 from
                          ; whatever context created them (e.g. a
                          ; timer ISR, which runs with interrupts off)
    mov  rdi, r13         ; arg
    call r12               ; entry(arg)

    call thread_exit      ; entry() returned — clean up, never returns
.hang:                    ; belt and suspenders if it somehow does
    cli
    hlt
    jmp .hang
