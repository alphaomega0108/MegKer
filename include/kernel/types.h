/* include/kernel/types.h
 * Fixed-width types for MegKer.
 * NEVER use int, long, short directly in kernel code —
 * their sizes change between architectures!
 */

#ifndef KERNEL_TYPES_H
#define KERNEL_TYPES_H

/* --- Unsigned integers --- */
typedef unsigned char        u8;
typedef unsigned short       u16;
typedef unsigned int         u32;
typedef unsigned long long   u64;

/* --- Signed integers --- */
typedef signed char          i8;
typedef signed short         i16;
typedef signed int           i32;
typedef signed long long     i64;

/* --- Pointer-sized types --- */
typedef u64                  usize;   /* size of a pointer on this arch */
typedef i64                  isize;

/* --- Common aliases --- */
typedef u8                   byte;
typedef u64                  physaddr;   /* physical memory address */
typedef u64                  virtaddr;   /* virtual memory address  */

/* --- Boolean --- */
typedef u8                   bool;
#define true                 1
#define false                0

/* --- NULL --- */
#define NULL                 ((void*)0)

/* --- Useful macros --- */
#define UNUSED(x)            ((void)(x))
#define ARRAY_SIZE(x)        (sizeof(x) / sizeof((x)[0]))
#define ALIGN_UP(x, align)   (((x) + (align) - 1) & ~((align) - 1))
#define ALIGN_DOWN(x, align) ((x) & ~((align) - 1))

/* --- Bit manipulation --- */
#define BIT(n)               (1ULL << (n))
#define BIT_SET(x, n)        ((x) |=  BIT(n))
#define BIT_CLR(x, n)        ((x) &= ~BIT(n))
#define BIT_TST(x, n)        ((x) &   BIT(n))

#endif /* KERNEL_TYPES_H */