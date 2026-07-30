/* arch/aarch64/syscall.c
 * Syscall ABI for EL0 processes, entered via `svc #0`
 * (boot/exceptions.S's svc_common_handler). Convention: syscall
 * number in x8, args in x0-x2, return value written back into x0 —
 * same shape as the real AArch64/Linux syscall ABI, picked for
 * familiarity rather than because anything here needs to match it.
 */

#include <arch/aarch64/process.h>
#include <arch/aarch64/regs.h>
#include <arch/aarch64/uart.h>
#include <kernel/types.h>

#define SYS_EXIT  0
#define SYS_WRITE 1   /* x0 = pointer to a NUL-terminated string */

void aarch64_svc_handler(aarch64_regs_t* regs)
{
    switch (regs->x8) {
    case SYS_EXIT:
        process_syscall_exit((i32)regs->x0);
        return;   /* never reached — control passes back to process_run() */

    case SYS_WRITE: {
        const char* s = (const char*)regs->x0;
        while (*s)
            uart_putc(*s++);
        regs->x0 = 0;
        break;
    }

    default:
        regs->x0 = (u64)-1;
        break;
    }
}
