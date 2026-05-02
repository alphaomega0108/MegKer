; arch/x86_64/boot/gdt_flush.asm
; Reloads CPU segment registers after GDT is updated.
; Called from gdt.c after lgdt instruction.

[BITS 64]
global gdt_flush

gdt_flush:
    ; RDI contains the GDT pointer (first argument in x86_64 calling convention)
    lgdt [rdi]

    ; Reload code segment via far return trick
    push 0x08               ; kernel code segment offset in GDT
    lea  rax, [rel .flush]
    push rax
    retfq                   ; far return → reloads CS

.flush:
    ; Reload all data segment registers
    mov ax, 0x10            ; kernel data segment offset in GDT
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret