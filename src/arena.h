// =================================================================================================
// bl
//
// File:   arena.h
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
	arena_elem_dtor_t   elem_dtor;
	mtx_t               lock;
};

void arena_init(struct arena     *arena,
                usize             elem_size_bytes,
                s32               elem_alignment,
                s32               elems_per_chunk,
                arena_elem_dtor_t elem_dtor);

void arena_terminate(struct arena *arena);

// Allocated memory is zero initialized.
void *arena_alloc(struct arena *arena);

#endif
