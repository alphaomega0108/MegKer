/* include/kernel/kernel.h
 * Core kernel definitions and version info.
 */

#ifndef KERNEL_H
#define KERNEL_H

#include <kernel/types.h>

/* --- Version --- */
#define KERNEL_NAME          "MegKer"
#define KERNEL_VERSION_MAJOR 0
#define KERNEL_VERSION_MINOR 1
#define KERNEL_VERSION_PATCH 0

/* --- Kernel state --- */
typedef enum {
    KERNEL_STATE_BOOT    = 0,   /* Still initializing   */
    KERNEL_STATE_RUNNING = 1,   /* Fully up             */
    KERNEL_STATE_PANIC   = 2,   /* Something went wrong */
    KERNEL_STATE_HALT    = 3    /* Shutting down        */
} kernel_state_t;

/* Readable from anywhere */
extern kernel_state_t kernel_state;

/* --- Core kernel functions --- */
void kernel_main(void);         /* main entry, called by arch boot */
void kernel_panic(const char* msg);  /* unrecoverable error */

#endif /* KERNEL_H */