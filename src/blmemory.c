// =================================================================================================
// bl
//
// File:   blmemory.c
// Author: Martin Dorazil
// Date:   21/6/20
//
// Copyright 2020 Martin Dorazil
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

#include "blmemory.h"
#include "common.h"

#if BL_RPMALLOC_ENABLE
#include "rpmalloc.h"

void *bl_realloc_impl(void *ptr, const size_t size, const char UNUSED(*filename), s32 UNUSED(line)) {
	zone();
	void *mem = rprealloc(ptr, size);
	if (!mem) abort();
	TracyCFree(ptr);
	TracyCAlloc(mem, size);
	return_zone(mem);
}

void *bl_malloc_impl(const size_t size, const char UNUSED(*filename), s32 UNUSED(line)) {
	zone();
	void *mem = rpmalloc(size);
	if (!mem) abort();
	TracyCAlloc(mem, size);
	return_zone(mem);
}

void bl_free_impl(void *ptr, const char UNUSED(*filename), s32 UNUSED(line)) {
	TracyCFree(ptr);
	rpfree(ptr);
}

void bl_alloc_init(void) {
	rpmalloc_initialize();
}

void bl_alloc_terminate(void) {
	rpmalloc_finalize();
}

void bl_alloc_thread_init(void) {
	rpmalloc_thread_initialize();
}

void bl_alloc_thread_terminate(void) {
	rpmalloc_thread_finalize(false);
}

#else

#include "TracyC.h"
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

void *bl_realloc_impl(void *ptr, const size_t size, const char UNUSED(*filename), s32 UNUSED(line)) {
	zone();
	void *mem = realloc(ptr, size);
	if (!mem) abort();
	TracyCFree(ptr);
	TracyCAlloc(mem, size);
	return_zone(mem);
}

void *bl_malloc_impl(const size_t size, const char UNUSED(*filename), s32 UNUSED(line)) {
	zone();
	void *mem = malloc(size);
	if (!mem) abort();
	TracyCAlloc(mem, size);
	return_zone(mem);
}

void bl_free_impl(void *ptr, const char UNUSED(*filename), s32 UNUSED(line)) {
	TracyCFree(ptr);
	free(ptr);
}

// UNUSED
void bl_alloc_init(void) {
}
void bl_alloc_terminate(void) {
}
void bl_alloc_thread_init(void) {
}
void bl_alloc_thread_terminate(void) {
}

#endif
