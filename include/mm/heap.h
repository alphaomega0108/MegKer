/* include/mm/heap.h
 * Kernel heap — general-purpose dynamic allocation for kernel code.
 * Arch-independent: built entirely on top of the PMM.
 */

#ifndef MM_HEAP_H
#define MM_HEAP_H

#include <kernel/types.h>

void  heap_init(void);
void* kmalloc(usize size);
void  kfree(void* ptr);

#endif /* MM_HEAP_H */
