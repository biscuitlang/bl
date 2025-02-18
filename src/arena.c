#include "arena.h"
#include "stb_ds.h"
#include "threading.h"

#define total_elem_size(A) ((A)->elem_size_bytes + (A)->elem_alignment)

struct arena_chunk {
	struct arena_chunk *next;
	s32                 count;
};

static inline struct arena_chunk *alloc_chunk(struct arena *arena) {
	zone();
	const usize         chunk_size = sizeof(struct arena_chunk) + total_elem_size(arena) * arena->elems_per_chunk;
	struct arena_chunk *chunk      = bmalloc(chunk_size);
	if (!chunk) babort("bad alloc");
	// bl_zeromem(chunk, chunk_size);
	chunk->count = 0;
	chunk->next  = NULL;
	return_zone(chunk);
}

static inline void *get_from_chunk(struct arena *arena, struct arena_chunk *chunk, s32 i) {
	bassert(i >= 0 && i < arena->elems_per_chunk);
	void     *elem = (void *)((char *)chunk + sizeof(struct arena_chunk) + i * total_elem_size(arena));
	ptrdiff_t adj;
	align_ptr_up(&elem, arena->elem_alignment, &adj);
	bassert(adj < arena->elem_alignment);
	return elem;
}

static inline struct arena_chunk *free_chunk(struct arena *arena, struct arena_chunk *chunk) {
	if (!chunk) return NULL;
	struct arena_chunk *next = chunk->next;
	if (arena->elem_dtor) {
		for (s32 i = 0; i < chunk->count; ++i) {
			arena->elem_dtor(get_from_chunk(arena, chunk, i));
		}
	}
	bfree(chunk);
	return next;
}

void arena_init(struct arena     *arena,
                usize             elem_size_bytes,
                s32               elem_alignment,
                s32               elems_per_chunk,
                u32               owner_thread_index,
                arena_elem_dtor_t elem_dtor) {
	arena->elem_size_bytes    = elem_size_bytes;
	arena->elems_per_chunk    = elems_per_chunk;
	arena->elem_alignment     = elem_alignment;
	arena->elem_dtor          = elem_dtor;
	arena->num_allocations    = 0;
	arena->owner_thread_index = owner_thread_index;

	// Preallocate right away...
	// arena->current_chunk = alloc_chunk(arena);
	// arena->first_chunk   = arena->current_chunk;
	arena->first_chunk   = NULL;
	arena->current_chunk = NULL;
}

void arena_terminate(struct arena *arena) {
	struct arena_chunk *chunk = arena->first_chunk;
	while (chunk) {
		chunk = free_chunk(arena, chunk);
	}
}

void *arena_alloc(struct arena *arena) {
	zone();
	bassert(arena->owner_thread_index == get_worker_index() && "Arena is supposed to be used from its initialization thread!");
	if (!arena->current_chunk) {
		arena->current_chunk = alloc_chunk(arena);
		arena->first_chunk   = arena->current_chunk;
	}

	if (arena->current_chunk->count == arena->elems_per_chunk) {
		// last chunk node
		struct arena_chunk *chunk  = alloc_chunk(arena);
		arena->current_chunk->next = chunk;
		arena->current_chunk       = chunk;
	}

	void *elem = get_from_chunk(arena, arena->current_chunk, arena->current_chunk->count++);
	bassert(is_aligned(elem, arena->elem_alignment) && "Unaligned allocation of arena element!");
	++arena->num_allocations;

	bl_zeromem(elem, arena->elem_size_bytes);

	return_zone(elem);
}

void arena_get_flatten(struct arena *arena, array(void *) * buf) {
	struct arena_chunk *chunk = arena->first_chunk;
	while (chunk) {
		arrsetcap(*buf, arrlen(*buf) + chunk->count);
		for (s32 i = 0; i < chunk->count; ++i) {
			void *ptr = get_from_chunk(arena, chunk, i);
			arrput(*buf, ptr);
		}
		chunk = chunk->next;
	}
}