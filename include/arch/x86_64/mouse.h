/* include/arch/x86_64/mouse.h
 * PS/2 mouse (i8042 auxiliary device, IRQ12), standard 3-byte packets.
 */

#ifndef ARCH_X86_64_MOUSE_H
#define ARCH_X86_64_MOUSE_H

#include <kernel/types.h>

void mouse_init(void);

/* Non-blocking: returns false if nothing changed since the last call. */
bool mouse_get_state(i32* x, i32* y, u8* buttons);

#endif /* ARCH_X86_64_MOUSE_H */
