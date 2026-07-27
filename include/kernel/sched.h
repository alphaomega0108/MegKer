/* include/kernel/sched.h
 * Cooperative-looking, actually-preemptive round-robin scheduler for
 * kernel threads. Arch-independent — the actual register save/
 * restore mechanics live behind arch_context_switch()/
 * arch_thread_init_stack() in arch.h.
 */

#ifndef KERNEL_SCHED_H
#define KERNEL_SCHED_H

#include <kernel/types.h>

typedef void (*thread_entry_t)(void* arg);

typedef enum {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_DEAD
} thread_state_t;

typedef struct thread {
    u64 id;
    void* sp;             /* saved stack pointer — meaning is arch-specific */
    void* stack_base;
    usize stack_size;
    thread_state_t state;
    struct thread* next;  /* ready-queue ring */
} thread_t;

void sched_init(void);                              /* current context becomes thread 0 */
thread_t* thread_create(thread_entry_t entry, void* arg);
thread_t* sched_current(void);

void sched_yield(void);   /* voluntary reschedule */
void sched_tick(void);    /* called from the timer IRQ — no-ops until sched_init() has run */

void thread_exit(void);   /* never returns */

#endif /* KERNEL_SCHED_H */
