/* include/arch/x86_64/process.h
 * Loads an ELF64 executable into its own address space and runs it
 * in ring 3.
 *
 * Synchronous and single-process for now: process_run() blocks
 * (from its caller's point of view — the rest of the kernel,
 * including other threads, keeps running) until the process calls
 * the exit syscall, then returns its exit code. Not yet integrated
 * with kernel/sched.c's thread ring; that's future work once there's
 * a reason to run more than one process at a time.
 */

#ifndef ARCH_X86_64_PROCESS_H
#define ARCH_X86_64_PROCESS_H

#include <kernel/types.h>

/* Returns the process's exit code, or -1 if the image couldn't be
 * loaded (bad magic/class/machine, no PT_LOAD segments, etc). */
i32 process_run(const void* elf_data, usize elf_size);

/* Called by the syscall dispatcher (arch/x86_64/syscall.c) to
 * terminate whatever process_run() currently has running. Never
 * returns to its caller — control passes back to process_run(). */
void process_syscall_exit(i32 code);

#endif /* ARCH_X86_64_PROCESS_H */
