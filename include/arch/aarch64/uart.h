/* include/arch/aarch64/uart.h
 * PL011 UART driver — QEMU's `virt` machine wires one at a fixed MMIO
 * address with no setup required, since QEMU pre-configures it.
 */

#ifndef ARCH_AARCH64_UART_H
#define ARCH_AARCH64_UART_H

#include <kernel/types.h>

void uart_init(void);
void uart_putc(char c);

#endif /* ARCH_AARCH64_UART_H */
