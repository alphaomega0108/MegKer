/* kernel/sched.c
 * Round-robin scheduler policy: a ring of threads, a "current"
 * pointer, and nothing cleverer than "next" for now (no priorities,
 * no load balancing — there's only ever one CPU running this so far).
 */

#include <kernel/sched.h>
#include <arch/arch.h>
#include <mm/heap.h>

#define THREAD_STACK_SIZE (16 * 1024)

static thread_t* current = NULL;
static u64 next_id = 0;
static bool sched_enabled = false;

void sched_init(void)
{
    /* The context already running (kernel_main's own call stack)
     * becomes thread 0 — no fake stack needed, arch_context_switch()
     * will capture its real sp the first time it's switched away
     * from. */
    thread_t* t = (thread_t*)kmalloc(sizeof(thread_t));
    t->id = next_id++;
    t->sp = NULL;
    t->stack_base = NULL;
    t->stack_size = 0;
    t->state = THREAD_RUNNING;
    t->next = t;

    current = t;
    sched_enabled = true;
}

thread_t* thread_create(thread_entry_t entry, void* arg)
{
    thread_t* t = (thread_t*)kmalloc(sizeof(thread_t));
    t->id = next_id++;
    t->stack_size = THREAD_STACK_SIZE;
    t->stack_base = kmalloc(THREAD_STACK_SIZE);
    t->sp = arch_thread_init_stack((u8*)t->stack_base + THREAD_STACK_SIZE, entry, arg);
    t->state = THREAD_READY;

    /* Splice into the ring right after current. */
    t->next = current->next;
    current->next = t;
    return t;
}

thread_t* sched_current(void)
{
    return current;
}

static void switch_to(thread_t* next)
{
    thread_t* prev = current;
    current = next;
    arch_context_switch(&prev->sp, next->sp);
}

void sched_yield(void)
{
    if (current->next != current)
        switch_to(current->next);
}

void sched_tick(void)
{
    if (!sched_enabled)
        return;
    sched_yield();
}

void thread_exit(void)
{
    thread_t* dead = current;
    dead->state = THREAD_DEAD;

    /* Unlink from the ring so it's never picked again. */
    thread_t* p = dead;
    while (p->next != dead)
        p = p->next;
    p->next = dead->next;

    thread_t* target = dead->next;
    current = target;
    arch_context_switch(&dead->sp, target->sp);
    /* never reached */
}
