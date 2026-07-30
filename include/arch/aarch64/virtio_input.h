/* include/arch/aarch64/virtio_input.h
 * virtio-input keyboard + mouse, on top of the generic virtio-mmio
 * transport (virtio.c). Mirrors arch/x86_64/keyboard.h + mouse.h's
 * combined shape so it plugs into arch_keyboard_getchar()/
 * arch_mouse_get_state() the same way.
 */

#ifndef ARCH_AARCH64_VIRTIO_INPUT_H
#define ARCH_AARCH64_VIRTIO_INPUT_H

#include <kernel/types.h>

void virtio_input_init(void);

char virtio_keyboard_getchar(void);
bool virtio_mouse_get_state(i32* x, i32* y, u8* buttons);

#endif /* ARCH_AARCH64_VIRTIO_INPUT_H */
