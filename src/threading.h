// =================================================================================================
// blc
//
// File:   threading.h
// Author: Martin Dorazil
// Date:   25/01/2022
//
// Copyright 2022 Martin Dorazil
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

#ifndef BL_THREADING_H
#define BL_THREADING_H

#include "common.h"
#include "config.h"
#include "tinycthread.h"

extern thrd_t MAIN_THREAD;

struct context;
struct mir_instr;

struct job_context {
	u32 thread_index;
	union {
		struct {
			struct assembly *assembly;
			struct unit     *unit;
		} unit;

		struct x64 {
			struct context   *ctx;
			struct mir_instr *top_instr;
		} x64;
	};
};

struct thread_local_storage {
	array(str_buf_t) temporary_strings;
};

typedef void (*job_fn_t)(struct job_context *ctx);

void start_threads(const s32 n);
void stop_threads(void);

// In single thread mode, all jobs are executed on caller thread (main thread) directly.
void wait_threads(void);

void submit_job(job_fn_t fn, struct job_context *ctx);

// Keeps all threads running, but process future jobs only on the main thread.
void set_single_thread_mode(const bool is_single);

// Returns 1 in single-thread mode and do not include main thread in multi-thread mode (main
// thread is just waiting for the rest to complete...).
//
// @Note 2024-09-09 This might change in future in case we implement processing of jobs on main
// thread while waiting for others.
u32 get_thread_count(void);

struct thread_local_storage *get_thread_local_storage(void);
void                         init_thread_local_storage(void);
void                         terminate_thread_local_storage(void);

#endif
