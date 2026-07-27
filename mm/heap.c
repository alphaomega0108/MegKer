/* mm/heap.c
 * First-fit kernel heap. Grows by requesting physically-contiguous
 * "arenas" from the PMM (pmm_alloc_frames) — each arena is one
 * independent first-fit free list, since blocks do pointer arithmetic
 * across the arena and can't span two unrelated PMM allocations.
 */

#include <mm/heap.h>
#include <mm/pmm.h>

#define HEAP_MIN_ARENA_FRAMES 16   /* 64KB, unless a bigger request needs more */
#define HEAP_ALIGN 8

typedef struct heap_block {
    usize size;                /* usable bytes, excludes this header */
    bool  free;
    struct heap_block* next;   /* next block in this arena, NULL at arena's end */
} heap_block_t;

typedef struct heap_arena {
    struct heap_arena* next;
    u64 frame_count;
    heap_block_t* first_block;
} heap_arena_t;

static heap_arena_t* arena_list = NULL;

static usize align_up(usize n, usize a)
{
    return (n + a - 1) & ~(a - 1);
}

static heap_arena_t* grow_heap(usize min_bytes)
{
    usize needed = sizeof(heap_arena_t) + sizeof(heap_block_t) + min_bytes;
    u64 frames = (needed + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE;
    if (frames < HEAP_MIN_ARENA_FRAMES)
        frames = HEAP_MIN_ARENA_FRAMES;

    physaddr base = pmm_alloc_frames(frames);
    if (base == 0)
        return NULL;

    heap_arena_t* arena = (heap_arena_t*)base;
    arena->frame_count = frames;

    heap_block_t* block = (heap_block_t*)(arena + 1);
    block->size = frames * PMM_FRAME_SIZE - sizeof(heap_arena_t) - sizeof(heap_block_t);
    block->free = true;
    block->next = NULL;
    arena->first_block = block;

    arena->next = arena_list;
    arena_list  = arena;
    return arena;
}

static void* alloc_from_block(heap_block_t* block, usize size)
{
    usize remainder = block->size - size;

    /* Only split off a new free block if there's enough room left for
     * it to be useful — otherwise just hand over the slack. */
    if (remainder > sizeof(heap_block_t) + HEAP_ALIGN) {
        heap_block_t* split = (heap_block_t*)((u8*)(block + 1) + size);
        split->size = remainder - sizeof(heap_block_t);
        split->free = true;
        split->next = block->next;

        block->size = size;
        block->next = split;
    }

    block->free = false;
    return (void*)(block + 1);
}

void heap_init(void)
{
    grow_heap(0);
}

void* kmalloc(usize size)
{
    if (size == 0)
        return NULL;

    usize aligned = align_up(size, HEAP_ALIGN);

    for (heap_arena_t* arena = arena_list; arena; arena = arena->next)
        for (heap_block_t* block = arena->first_block; block; block = block->next)
            if (block->free && block->size >= aligned)
                return alloc_from_block(block, aligned);

    heap_arena_t* arena = grow_heap(aligned);
    if (!arena)
        return NULL;   /* out of physical memory */

    return alloc_from_block(arena->first_block, aligned);
}

void kfree(void* ptr)
{
    if (!ptr)
        return;

    heap_block_t* block = (heap_block_t*)ptr - 1;
    block->free = true;

    /* Forward-only coalescing — blocks don't carry a prev pointer. */
    if (block->next && block->next->free) {
        block->size += sizeof(heap_block_t) + block->next->size;
        block->next = block->next->next;
    }
}
