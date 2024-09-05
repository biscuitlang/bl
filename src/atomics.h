// =================================================================================================
// blc
//
// File:   atomics.h
// Author: Martin Dorazil
// Date:   2024-09-05
//
// Copyright 2024 Martin Dorazil
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

#ifndef BL_ATOMICS_H
#define BL_ATOMICS_H

// Atomics
#if BL_PLATFORM_WIN
#	include <Windows.h>

#	define batomic_store(a, val) InterlockedExchange((a), (val));
#	define batomic_load(a) InterlockedCompareExchange((a), 0, 0)
#	define batomic_fetch_add(a, val) InterlockedAdd((a), (val))

typedef volatile LONG batomic_int;

#else
#	include <stdatomic.h>

#	define batomic_store(a, val) atomic_store((a), (val))
#	define batomic_load(a) atomic_load((a))
#	define batomic_fetch_add(a, val) atomic_fetch_add((a), (val))

typedef atomic_int batomic_int;

#endif

#endif
