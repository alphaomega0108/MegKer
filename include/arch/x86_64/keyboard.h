/* include/arch/x86_64/keyboard.h
 * PS/2 keyboard driver (i8042 controller, IRQ1, scan code set 1 —
 * what QEMU and every PC chipset since emulates by default).
 */

#ifndef ARCH_X86_64_KEYBOARD_H
#define ARCH_X86_64_KEYBOARD_H

#include <kernel/types.h>

void keyboard_init(void);

/* Returns the next buffered character, or 0 if none is waiting.
 * Non-blocking — callers poll. */
char keyboard_getchar(void);

#endif /* ARCH_X86_64_KEYBOARD_H */
