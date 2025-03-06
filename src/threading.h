#ifndef BL_THREADING_H
#define BL_THREADING_H

#include "common.h"
#include "config.h"
#include "tinycthread.h"

extern thrd_t MAIN_THREAD;

struct context;
struct mir_instr;

struct job_context {
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
#if BL_ASSERT_ENABLE
	s32 _temporary_strings_check;
#endif
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

// Resolve index used for the current worker thread.
u32 get_worker_index(void);

struct thread_local_storage *get_thread_local_storage(void);
void                         init_thread_local_storage(void);
void                         terminate_thread_local_storage(void);

#endif
