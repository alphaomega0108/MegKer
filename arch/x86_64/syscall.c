/* arch/x86_64/syscall.c
 * Syscall ABI for ring-3 processes, entered via `int 0x80`
 * (arch/x86_64/boot/isr_stubs.asm's isr128, dispatched here from
 * idt.c's isr_handler). Convention: syscall number in rax, args in
 * rdi/rsi/rdx/r10, return value written back into rax.
 */

#include <arch/x86_64/idt.h>
#include <arch/x86_64/framebuffer.h>
#include <arch/x86_64/process.h>
#include <kernel/types.h>

#define SYS_EXIT       0
#define SYS_DEBUG_DRAW 1   /* rdi=x, rsi=y, rdx=string ptr, r10=0x00RRGGBB color */

void syscall_dispatch(registers_t* regs)
{
    switch (regs->rax) {
    case SYS_EXIT:
        process_syscall_exit((i32)regs->rdi);
        return;   /* never reached — control passes back to process_run() */

    case SYS_DEBUG_DRAW:
        if (fb_available())
            fb_draw_string((u32)regs->rdi, (u32)regs->rsi,
                            (const char*)regs->rdx, (u32)regs->r10, 0x00000000);
        regs->rax = 0;
        break;

    default:
        regs->rax = (u64)-1;
        break;
    }
}
