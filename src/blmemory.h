// =================================================================================================
// bl
//
// File:   blmemory.h
// Author: Martin Dorazil
// Date:   3/1/18
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

#ifndef BL_BLMEMORY_H
#define BL_BLMEMORY_H

#include "basic_types.h"
#include "config.h"

#define bmalloc(size) bl_malloc_impl(size, __FILE__, __LINE__)
#define brealloc(ptr, size) bl_realloc_impl(ptr, size, __FILE__, __LINE__)
#define bfree(ptr) bl_free_impl(ptr, __FILE__, __LINE__)

// These might not be implemented in some cases...
void bl_alloc_init(void);
void bl_alloc_terminate(void);
void bl_alloc_thread_init(void);
void bl_alloc_thread_terminate(void);

void *bl_realloc_impl(void *ptr, const size_t size, const char *filename, s32 line);
void *bl_malloc_impl(const size_t size, const char *filename, s32 line);
void  bl_free_impl(void *ptr, const char *filename, s32 line);
void *bl_zeromem(void *dest, usize size);

#endif // BL_BLMEMORY_H
