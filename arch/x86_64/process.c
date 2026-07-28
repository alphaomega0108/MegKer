/* arch/x86_64/process.c
 * Minimal ELF64 loader + ring-3 launcher.
 *
 * Launching a process reuses the exact same suspend/resume mechanism
 * as kernel threads (arch_context_switch(), see context_switch.asm):
 * a small "launch stack" gets a synthetic initial frame whose `ret`
 * lands in process_trampoline (usermode.asm), which unpacks its
 * arguments and calls enter_usermode() to iretq into ring 3. Exiting
 * is the mirror image — the exit syscall calls
 * arch_context_switch() again to jump straight back to process_run(),
 * right after *its* call, abandoning the pending syscall entirely
 * rather than iretq-ing back to ring 3.
 *
 * Known limitations, honestly: only one process can run at a time
 * (this isn't threaded into kernel/sched.c's ready queue), and
 * syscall arguments/pointers from ring 3 aren't validated or copied
 * — safe only because the process's address space is a full clone
 * of the kernel's own, so a bad pointer lands somewhere mapped
 * rather than faulting, but a malicious or buggy program could still
 * corrupt kernel-visible memory. Real "copy from/to user" validation
 * is future work, alongside real multi-process scheduling.
 */

#include <arch/x86_64/process.h>
#include <arch/x86_64/vmm.h>
#include <arch/arch.h>
#include <mm/pmm.h>
#include <mm/heap.h>
#include <kernel/types.h>

#define USER_CODE_SEL 0x1B   /* GDT index 3, RPL 3 */
#define USER_DATA_SEL 0x23   /* GDT index 4, RPL 3 */

#define USER_STACK_TOP    0x00007FFFFFFFF000ULL
#define USER_STACK_PAGES  4
#define PAGE_SIZE         0x1000ULL

#define LAUNCH_STACK_SIZE (16 * 1024)

typedef struct {
    u8  e_ident[16];
    u16 e_type;
    u16 e_machine;
    u32 e_version;
    u64 e_entry;
    u64 e_phoff;
    u64 e_shoff;
    u32 e_flags;
    u16 e_ehsize;
    u16 e_phentsize;
    u16 e_phnum;
    u16 e_shentsize;
    u16 e_shnum;
    u16 e_shstrndx;
} __attribute__((packed)) elf64_ehdr_t;

typedef struct {
    u32 p_type;
    u32 p_flags;
    u64 p_offset;
    u64 p_vaddr;
    u64 p_paddr;
    u64 p_filesz;
    u64 p_memsz;
    u64 p_align;
} __attribute__((packed)) elf64_phdr_t;

#define PT_LOAD      1
#define ELFCLASS64   2
#define EM_X86_64    62
#define PF_W         2   /* ELF program header flag: writable */

typedef struct {
    u64 entry;
    u64 user_stack;
    u64 data_sel;
    u64 code_sel;
} launch_params_t;

extern void process_trampoline(void);

static void* kernel_return_sp;
static i32   process_exit_code;

static void* build_launch_stack(void* stack_top, launch_params_t* params)
{
    u64* sp = (u64*)stack_top;

    *(--sp) = (u64)process_trampoline;  /* landed on via `ret` in arch_context_switch */
    *(--sp) = 0;                        /* rbp */
    *(--sp) = 0;                        /* rbx */
    *(--sp) = (u64)params;              /* r12 — read by process_trampoline */
    *(--sp) = 0;                        /* r13 */
    *(--sp) = 0;                        /* r14 */
    *(--sp) = 0;                        /* r15 */

    return sp;
}

static bool elf_valid(const elf64_ehdr_t* eh, usize size)
{
    if (size < sizeof(elf64_ehdr_t))
        return false;
    if (eh->e_ident[0] != 0x7F || eh->e_ident[1] != 'E' ||
        eh->e_ident[2] != 'L'  || eh->e_ident[3] != 'F')
        return false;
    if (eh->e_ident[4] != ELFCLASS64)
        return false;
    if (eh->e_machine != EM_X86_64)
        return false;
    return true;
}

i32 process_run(const void* elf_data, usize elf_size)
{
    const elf64_ehdr_t* eh = (const elf64_ehdr_t*)elf_data;
    if (!elf_valid(eh, elf_size))
        return -1;

    address_space_t* as = vmm_create_address_space();
    const u8* base = (const u8*)elf_data;
    const elf64_phdr_t* ph = (const elf64_phdr_t*)(base + eh->e_phoff);

    for (u16 i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD)
            continue;

        u64 vstart = ALIGN_DOWN(ph[i].p_vaddr, PAGE_SIZE);
        u64 vend   = ALIGN_UP(ph[i].p_vaddr + ph[i].p_memsz, PAGE_SIZE);
        u64 seg_flags = VMM_PRESENT | VMM_USER;
        if (ph[i].p_flags & PF_W)
            seg_flags |= VMM_WRITABLE;

        for (u64 va = vstart; va < vend; va += PAGE_SIZE) {
            physaddr frame = pmm_alloc_frame();
            u8* p = (u8*)frame;
            for (int b = 0; b < 4096; b++)
                p[b] = 0;
            vmm_map(as, va, frame, seg_flags);
        }

        /* Copy the segment's file bytes in. Physical frames below
         * 1GB are identity-mapped in every address space (including
         * the kernel's own, currently active one), so writing via
         * the physical address works regardless of which CR3 is
         * loaded right now. */
        for (u64 off = 0; off < ph[i].p_filesz; off++) {
            physaddr pa = vmm_translate(as, ph[i].p_vaddr + off);
            *(u8*)pa = base[ph[i].p_offset + off];
        }
    }

    for (u32 i = 0; i < USER_STACK_PAGES; i++) {
        physaddr frame = pmm_alloc_frame();
        vmm_map(as, USER_STACK_TOP - (u64)(i + 1) * PAGE_SIZE, frame,
                VMM_PRESENT | VMM_WRITABLE | VMM_USER);
    }

    launch_params_t params = {
        .entry      = eh->e_entry,
        .user_stack = USER_STACK_TOP,
        .data_sel   = USER_DATA_SEL,
        .code_sel   = USER_CODE_SEL,
    };
    void* launch_stack_base = kmalloc(LAUNCH_STACK_SIZE);
    void* launch_sp = build_launch_stack((u8*)launch_stack_base + LAUNCH_STACK_SIZE, &params);

    vmm_switch(as);
    arch_context_switch(&kernel_return_sp, launch_sp);
    /* Resumes here once the process calls the exit syscall. That
     * syscall's interrupt gate cleared IF on entry, and we never
     * iretq to restore it — process_syscall_exit() abandons that
     * interrupt entirely via arch_context_switch() instead of
     * returning through it. Restore it explicitly, or interrupts
     * (and everything timer-driven — the scheduler, gui_update()'s
     * redraw throttle) stay silently disabled from here on. */
    __asm__ volatile ("sti");
    vmm_switch(vmm_kernel_space());

    vmm_destroy_address_space(as);
    kfree(launch_stack_base);

    return process_exit_code;
}

void process_syscall_exit(i32 code)
{
    process_exit_code = code;

    void* unused;
    arch_context_switch(&unused, kernel_return_sp);
    /* never reached */
}
