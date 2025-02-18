#ifndef BL_ARENA_H
#define BL_ARENA_H

#include "common.h"
#include "tinycthread.h"

typedef void (*arena_elem_dtor_t)(void *);

struct arena_chunk;

// 2024-08-10 Arenas are by default thread safe.
struct arena {
	struct arena_chunk *first_chunk;
	struct arena_chunk *current_chunk;
	usize               elem_size_bytes;
	s32                 elem_alignment;
	s32                 elems_per_chunk;
	usize               num_allocations;
	u32                 owner_thread_index;

	arena_elem_dtor_t elem_dtor;
};

void arena_init(struct arena     *arena,
                usize             elem_size_bytes,
                s32               elem_alignment,
                s32               elems_per_chunk,
                u32               owner_thread_index,
                arena_elem_dtor_t elem_dtor);

void arena_terminate(struct arena *arena);

// Allocated memory is zero initialized.
void *arena_alloc(struct arena *arena);

// This might be expensive and should be used in special cases for debugging.
void arena_get_flatten(struct arena *arena, array(void *) * buf);

#endif
