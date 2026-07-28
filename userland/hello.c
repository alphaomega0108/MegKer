/* userland/hello.c
 * Tiny freestanding ring-3 test program — no libc, raw syscalls via
 * `int 0x80`. Built as a real static ELF64 executable and embedded
 * into the kernel image (see the Makefile) to exercise the ELF
 * loader and syscall path end to end, not just a hardcoded stub.
 *
 * Syscall ABI (see arch/x86_64/syscall.c): number in rax, args in
 * rdi/rsi/rdx/r10, return value in rax.
 */

#define SYS_EXIT       0
#define SYS_DEBUG_DRAW 1

static long syscall4(long num, long a1, long a2, long a3, long a4)
{
    long ret;
    register long r10 __asm__("r10") = a4;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "D"(a1), "S"(a2), "d"(a3), "r"(r10)
        : "memory"
    );
    return ret;
}

void _start(void)
{
    syscall4(SYS_DEBUG_DRAW, 40, 300, (long)"HELLO FROM RING 3", 0x0000FF00);
    syscall4(SYS_EXIT, 0, 0, 0, 0);

    for (;;) { }   /* never reached */
}
