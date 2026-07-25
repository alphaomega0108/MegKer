/* include/arch/x86_64/io.h
 * Raw port I/O — talks to the PIC, PIT, and other legacy devices
 * that live on the x86 I/O bus rather than in memory.
 */

#ifndef ARCH_X86_64_IO_H
#define ARCH_X86_64_IO_H

#include <kernel/types.h>

static inline void outb(u16 port, u8 val)
{
    __asm__ volatile ("outb %0, %1" :: "a"(val), "Nd"(port));
}

static inline u8 inb(u16 port)
{
    u8 ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* Old PICs need a small delay between commands on real hardware —
 * writing to an unused port takes about as long as it needs to. */
static inline void io_wait(void)
{
    outb(0x80, 0);
}

#endif /* ARCH_X86_64_IO_H */
