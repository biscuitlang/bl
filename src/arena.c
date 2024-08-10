// =================================================================================================
// bl
//
// File:   arena.c
// Author: Martin Dorazil
// Date:   3/14/18
//
// Copyright 2018 Martin Dorazil
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
// =================================================================================================

#include "arena.h"
#include "threading.h"

#define total_elem_size(A) ((A)->elem_size_bytes + (A)->elem_alignment)

struct arena_chunk {
	struct arena_chunk *next;
	s32                 count;
};

struct arena_sync_impl {
	pthread_spinlock_t lock;
};

static struct arena_sync_impl *sync_new(void) {
	struct arena_sync_impl *s = bmalloc(sizeof(struct arena_sync_impl));
	pthread_spin_init(&s->lock, 0);
	return s;
}

static void sync_delete(struct arena_sync_impl *s) {
	pthread_spin_destroy(&s->lock);
	bfree(s);
}

static inline struct arena_chunk *alloc_chunk(struct arena *arena) {
	zone();
	const usize chunk_size =
	    sizeof(struct arena_chunk) + total_elem_size(arena) * arena->elems_per_chunk;
	struct arena_chunk *chunk = bmalloc(chunk_size);
	if (!chunk) babort("bad alloc");
	bl_zeromem(chunk, chunk_size);
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
                arena_elem_dtor_t elem_dtor) {
	arena->elem_size_bytes = elem_size_bytes;
	arena->elems_per_chunk = elems_per_chunk;
	arena->elem_alignment  = elem_alignment;
	arena->first_chunk     = NULL;
	arena->current_chunk   = NULL;
	arena->elem_dtor       = elem_dtor;
	arena->sync            = sync_new();
}

void arena_terminate(struct arena *arena) {
	struct arena_chunk *chunk = arena->first_chunk;
	while (chunk) {
		chunk = free_chunk(arena, chunk);
	}
	sync_delete(arena->sync);
}

void *arena_alloc(struct arena *arena) {
	zone();
	pthread_spin_lock(&arena->sync->lock);
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
	pthread_spin_unlock(&arena->sync->lock);
	return_zone(elem);
}
