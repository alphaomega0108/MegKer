/* userland/hello_aarch64.c
 * Tiny freestanding EL0 test program — no libc, raw syscalls via
 * `svc #0`. Built as a real static ELF64 executable and embedded
 * into the kernel image (see the Makefile) to exercise the ELF
 * loader and syscall path end to end, not just a hardcoded stub.
 *
 * Syscall ABI (see arch/aarch64/syscall.c): number in x8, args in
 * x0-x2, return value in x0.
 */

#define SYS_EXIT  0
#define SYS_WRITE 1

static long syscall1(long num, long a0)
{
    register long x8 __asm__("x8") = num;
    register long x0 __asm__("x0") = a0;
    __asm__ volatile (
        "svc #0"
        : "+r" (x0)
        : "r" (x8)
        : "memory"
    );
    return x0;
}

void _start(void)
{
    syscall1(SYS_WRITE, (long)"HELLO FROM EL0\n");
    syscall1(SYS_EXIT, 0);

    for (;;) { }   /* never reached */
}
