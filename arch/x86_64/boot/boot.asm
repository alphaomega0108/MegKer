; arch/x86_64/boot/boot.asm
;
; MegKer x86_64 boot entry point.
; GRUB loads us here in 32-bit Protected Mode.
; Our job: verify multiboot, set up a stack,
; switch to 64-bit Long Mode, call kernel_main.
;
; Assembled with NASM.

; ─────────────────────────────────────────────
; MULTIBOOT2 HEADER
; This magic header tells GRUB "this is a kernel"
; It MUST be in the first 8KB of the binary
; ─────────────────────────────────────────────
section .multiboot
align 8

MULTIBOOT2_MAGIC    equ 0xE85250D6
MULTIBOOT2_ARCH     equ 0           ; 0 = x86/x86_64
MULTIBOOT2_LENGTH   equ (mb2_end - mb2_start)
MULTIBOOT2_CHECKSUM equ -(MULTIBOOT2_MAGIC + MULTIBOOT2_ARCH + MULTIBOOT2_LENGTH)

mb2_start:
    dd MULTIBOOT2_MAGIC
    dd MULTIBOOT2_ARCH
    dd MULTIBOOT2_LENGTH
    dd MULTIBOOT2_CHECKSUM

    ; End tag — required by multiboot2 spec
    dw 0    ; type = 0 (end)
    dw 0    ; flags
    dd 8    ; size
mb2_end:

; ─────────────────────────────────────────────
; BSS SECTION — Uninitialized data
; We put our stack and page tables here
; ─────────────────────────────────────────────
section .bss.boot
align 16

; Page tables (needed for Long Mode)
pml4_table: resb 4096   ; Page Map Level 4
pdp_table:  resb 4096   ; Page Directory Pointer
pd_table:   resb 4096   ; Page Directory

; Stack — grows downward from stack_top
stack_bottom:
    resb 16384          ; 16KB kernel stack
stack_top:

; ─────────────────────────────────────────────
; TEXT SECTION — Actual code
; ─────────────────────────────────────────────
section .text._start
bits 32

; This is what GRUB jumps to
global _start
_start:
    ; 1. Disable interrupts immediately
    ;    We are not ready to handle them yet
    cli

    ; 2. Set up our stack
    ;    ESP points to top of our 16KB stack
    mov esp, stack_top

    ; 3. Save multiboot info pointer
    ;    GRUB puts magic in EAX, info ptr in EBX
    ;    We save EBX before we trash registers
    push ebx            ; multiboot info pointer
    push eax            ; multiboot magic number

    ; 4. Check if CPU supports Long Mode (64-bit)
    call check_long_mode

    ; 5. Set up page tables for Long Mode
    call setup_page_tables

    ; 6. Enable PAE (Physical Address Extension)
    ;    Required before entering Long Mode
    mov eax, cr4
    or  eax, (1 << 5)   ; Set PAE bit
    mov cr4, eax

    ; 7. Point CR3 to our PML4 table
    mov eax, pml4_table
    mov cr3, eax

    ; 8. Enable Long Mode in the EFER MSR
    mov ecx, 0xC0000080         ; EFER MSR number
    rdmsr                        ; read it
    or  eax, (1 << 8)           ; set LME (Long Mode Enable)
    wrmsr                        ; write it back

    ; 9. Enable paging + protected mode
    mov eax, cr0
    or  eax, (1 << 31) | (1 << 0)  ; PG + PE bits
    mov cr0, eax

    ; 10. Far jump to load 64-bit GDT segment
    ;     This actually activates Long Mode
    lgdt [gdt64.pointer]
    jmp  gdt64.code_segment:long_mode_entry

; ─────────────────────────────────────────────
; CHECK LONG MODE
; Panics (halts) if CPU can't do 64-bit
; ─────────────────────────────────────────────
check_long_mode:
    ; Check if CPUID is supported
    ; Try to flip bit 21 in EFLAGS
    pushfd
    pop  eax
    mov  ecx, eax
    xor  eax, (1 << 21)
    push eax
    popfd
    pushfd
    pop  eax
    push ecx
    popfd
    cmp  eax, ecx
    je   .no_long_mode      ; bit didn't flip = no CPUID

    ; Now check if Long Mode is available via CPUID
    mov  eax, 0x80000000
    cpuid
    cmp  eax, 0x80000001
    jb   .no_long_mode      ; extended CPUID not supported

    mov  eax, 0x80000001
    cpuid
    test edx, (1 << 29)     ; LM bit
    jz   .no_long_mode      ; Long Mode not supported

    ret                     ; All good!

.no_long_mode:
    ; Print "NO64" to VGA memory so we know what happened
    mov dword [0xB8000], 0x4F4F4F4E  ; 'NO' in red
    mov dword [0xB8004], 0x4F34304F  ; '04' in red
    hlt

; ─────────────────────────────────────────────
; SET UP PAGE TABLES
; We use 2MB huge pages for simplicity
; Maps first 1GB of physical memory
; ─────────────────────────────────────────────
setup_page_tables:
    ; PML4[0] → PDP table
    mov eax, pdp_table
    or  eax, 0b11          ; present + writable
    mov [pml4_table], eax

    ; PDP[0] → PD table
    mov eax, pd_table
    or  eax, 0b11          ; present + writable
    mov [pdp_table], eax

    ; Map 512 entries in PD = 512 x 2MB = 1GB
    mov ecx, 0              ; counter

.map_pd:
    ; Each entry maps a 2MB page
    ; address = ecx * 2MB
    mov eax, 0x200000       ; 2MB
    mul ecx
    or  eax, 0b10000011     ; present + writable + huge page
    mov [pd_table + ecx * 8], eax

    inc ecx
    cmp ecx, 512
    jne .map_pd

    ret

; ─────────────────────────────────────────────
; 64-BIT GDT
; Minimal GDT required for Long Mode
; ─────────────────────────────────────────────
section .rodata
gdt64:
.null_segment:
    dq 0                    ; Null descriptor (required)

.code_segment: equ $ - gdt64
    dq (1 << 43) | (1 << 44) | (1 << 47) | (1 << 53)
    ; bits: executable | code | present | 64-bit

.data_segment: equ $ - gdt64
    dq (1 << 44) | (1 << 47) | (1 << 41)
    ; bits: data | present | writable

.pointer:
    dw $ - gdt64 - 1        ; GDT size - 1
    dq gdt64                ; GDT address

; ─────────────────────────────────────────────
; LONG MODE ENTRY
; We are now in 64-bit mode!
; ─────────────────────────────────────────────
section .text
bits 64

long_mode_entry:
    ; Load data segment registers
    mov ax, gdt64.data_segment
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Restore multiboot info (we pushed it earlier)
    pop  rdi            ; multiboot magic   → 1st argument
    pop  rsi            ; multiboot info    → 2nd argument

    ; Save them across the BSS-zero loop below (it needs a scratch reg)
    push rdi
    push rsi

    ; Zero out BSS section
    ; (C expects uninitialized globals to be 0)
    extern _bss_start
    extern _bss_end
    mov  rax, _bss_start
.zero_bss:
    cmp  rax, _bss_end
    jge  .bss_done
    mov  byte [rax], 0
    inc  rax
    jmp  .zero_bss
.bss_done:

    ; Restore RDI and RSI after BSS zero
    pop  rsi
    pop  rdi

    ; Call kernel_main() — never returns
    extern kernel_main
    call kernel_main

    ; If kernel_main somehow returns — halt
    cli
    hlt