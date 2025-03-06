#include "threading.h"
#include "stb_ds.h"

thrd_t MAIN_THREAD = (thrd_t)0;

static _Thread_local struct thread_local_storage thread_data;
static _Thread_local u32                         worker_index = 0; // By default 0 for main thread.

struct job {
	struct job_context ctx;
	job_fn_t           fn;
};

static array(struct job) jobs;
static mtx_t jobs_mutex;
static cnd_t jobs_cond;
static cnd_t working_cond;
static s32   jobs_running     = 0;
static s32   thread_count     = 0;
static bool  should_exit      = false;
static bool  is_single_thread = false;

static bool pop_job(struct job *job) {
	s64 len = arrlen(jobs);
	if (len == 0) return false;
	memcpy(job, &jobs[len - 1], sizeof(struct job));
	--len;
	bassert(len >= 0);
	arrsetlen(jobs, len);
	return true;
}

static s32 worker(void *args) {
	worker_index = (u32)(u64)args;

	bl_alloc_thread_init();
	init_thread_local_storage();
	struct job job;

	while (true) {
		mtx_lock(&jobs_mutex);
		while (arrlenu(jobs) == 0 && !should_exit)
			cnd_wait(&jobs_cond, &jobs_mutex);

		if (should_exit)
			break;

		const bool has_job = pop_job(&job);
		++jobs_running;
		bassert(jobs_running <= thread_count);
		mtx_unlock(&jobs_mutex);

		// Execute the job
		if (has_job) job.fn(&job.ctx);

		mtx_lock(&jobs_mutex);
		--jobs_running;
		bassert(jobs_running >= 0);

		// Might signal anyone waiting for the submitted batch to complete.
		if (!should_exit && jobs_running == 0 && arrlenu(jobs) == 0)
			cnd_signal(&working_cond);
		mtx_unlock(&jobs_mutex);
	}

	bassert(thread_count > 0);

	--thread_count;
	cnd_signal(&working_cond);
	mtx_unlock(&jobs_mutex);

	terminate_thread_local_storage();
	bl_alloc_thread_terminate();
	return 0;
}

// =================================================================================================
// PUBLIC
// =================================================================================================

void start_threads(const s32 n) {
	bassert(n > 1);
	bassert(thread_count == 0 && "Thread pool is already running!");
	thread_count     = n;
	is_single_thread = false;

	mtx_init(&jobs_mutex, mtx_plain);
	cnd_init(&jobs_cond);
	cnd_init(&working_cond);

	for (s32 i = 0; i < thread_count; ++i) {
		thrd_t thread = 0;
		thrd_create(&thread, &worker, (void *)(u64)i);
		thrd_detach(thread);
	}
}

void stop_threads(void) {
	mtx_lock(&jobs_mutex);
	arrsetlen(jobs, 0);
	should_exit = true;
	cnd_broadcast(&jobs_cond);
	mtx_unlock(&jobs_mutex);

	wait_threads();

	cnd_destroy(&working_cond);
	cnd_destroy(&jobs_cond);
	mtx_destroy(&jobs_mutex);

	arrfree(jobs);

	thread_count = 0;
}

void wait_threads(void) {
	if (is_single_thread) {
		struct job job;
		while (pop_job(&job)) {
			job.fn(&job.ctx);
		}
		return;
	}

	mtx_lock(&jobs_mutex);
	while ((!should_exit && (arrlenu(jobs) > 0 || jobs_running)) || (should_exit && thread_count != 0)) {
		cnd_wait(&working_cond, &jobs_mutex);
	}
	mtx_unlock(&jobs_mutex);
	if (arrlenu(jobs) != 0 && should_exit == false) {
		babort("Parallel compilation failed, not all jobs were completed as expected.");
	}
}

void submit_job(job_fn_t fn, struct job_context *ctx) {
	if (!is_single_thread) {
		mtx_lock(&jobs_mutex);
	}
	bassert(fn);
	struct job *job = arraddnptr(jobs, 1);
	if (ctx) {
		// Note in case we have no context, we leave the job's cxt uninitialized!
		memcpy(&job->ctx, ctx, sizeof(struct job_context));
	}
	job->fn = fn;

	if (!is_single_thread) {
		cnd_broadcast(&jobs_cond);
		mtx_unlock(&jobs_mutex);
	}
}

void set_single_thread_mode(const bool is_single) {
	if (is_single_thread == is_single) return;
	wait_threads();
	is_single_thread = is_single;
}

u32 get_thread_count(void) {
	if (is_single_thread) return 1;
	return thread_count;
}

struct thread_local_storage *get_thread_local_storage(void) {
	return &thread_data;
}

void init_thread_local_storage(void) {
	arrsetcap(thread_data.temporary_strings, 16);
#if BL_ASSERT_ENABLE
	thread_data._temporary_strings_check = 0;
#endif
}

void terminate_thread_local_storage(void) {
	for (usize i = 0; i < arrlenu(thread_data.temporary_strings); ++i) {
		str_buf_free(&thread_data.temporary_strings[i]);
	}
	arrfree(thread_data.temporary_strings);
	bassert(thread_data._temporary_strings_check == 0 && "We have dangling temporary strings!");
}

u32 get_worker_index(void) {
	return worker_index;
}