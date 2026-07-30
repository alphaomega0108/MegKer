/* arch/aarch64/drivers/uart.c
 * PL011 UART, polled — QEMU's `virt` machine maps one at 0x09000000
 * and pre-configures it (baud rate, enable) before the guest ever
 * runs, so there's nothing to set up beyond waiting on the FIFO.
 */

#include <arch/aarch64/uart.h>

#define UART0_BASE 0x09000000UL

#define UART_DR  (*(volatile u32*)(UART0_BASE + 0x00))
#define UART_FR  (*(volatile u32*)(UART0_BASE + 0x18))
#define UART_CR  (*(volatile u32*)(UART0_BASE + 0x30))

#define UART_FR_TXFF (1u << 5)   /* transmit FIFO full */

#define UART_CR_UARTEN (1u << 0)
#define UART_CR_TXE    (1u << 8)
#define UART_CR_RXE    (1u << 9)

void uart_init(void)
{
    /* QEMU's reset state has TXE/RXE on but UARTEN off — writes still
     * reach the host in that state (QEMU just logs a warning), but
     * enabling it properly is the real PL011 contract and doesn't
     * depend on that leniency. */
    UART_CR = UART_CR_UARTEN | UART_CR_TXE | UART_CR_RXE;
}

void uart_putc(char c)
{
    while (UART_FR & UART_FR_TXFF) { }
    UART_DR = (u32)(u8)c;
}
