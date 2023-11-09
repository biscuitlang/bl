// =================================================================================================
// bl
//
// File:   builder.h
// Author: Martin Dorazil
// Date:   02/03/2018
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

#include "builder.h"
#include "conf.h"
#include "stb_ds.h"
#include "threading.h"
#include "vmdbg.h"
#include <stdarg.h>

#if !BL_PLATFORM_WIN
#	include <unistd.h>
#endif

struct builder builder;

// =================================================================================================
// Stages
// =================================================================================================

void file_loader_run(struct assembly *assembly, struct unit *unit);
void lexer_run(struct assembly *assembly, struct unit *unit);
void token_printer_run(struct assembly *assembly, struct unit *unit);
void parser_run(struct assembly *assembly, struct unit *unit);
void ast_printer_run(struct assembly *assembly);
void docs_run(struct assembly *assembly);
void ir_run(struct assembly *assembly);
void ir_opt_run(struct assembly *assembly);
void obj_writer_run(struct assembly *assembly);
void linker_run(struct assembly *assembly);
void bc_writer_run(struct assembly *assembly);
void native_bin_run(struct assembly *assembly);
void mir_writer_run(struct assembly *assembly);
void asm_writer_run(struct assembly *assembly);

// Virtual Machine
void vm_entry_run(struct assembly *assembly);
void vm_build_entry_run(struct assembly *assembly);
void vm_tests_run(struct assembly *assembly);

const char *supported_targets[] = {
#define GEN_SUPPORTED
#include "target.def"
#undef GEN_SUPPORTED
};

const char *supported_targets_experimental[] = {
#define GEN_EXPERIMENTAL
#include "target.def"
#undef GEN_EXPERIMENTAL
};

struct builder_sync_impl {
	pthread_mutex_t log_mutex;
};

static struct builder_sync_impl *sync_new(void) {
	struct builder_sync_impl *impl = bmalloc(sizeof(struct builder_sync_impl));
	pthread_mutex_init(&impl->log_mutex, NULL);
	return impl;
}

static void sync_delete(struct builder_sync_impl *impl) {
	pthread_mutex_destroy(&impl->log_mutex);
	bfree(impl);
}

// =================================================================================================
// Builder
// =================================================================================================
static int  compile_unit(struct unit *unit, struct assembly *assembly);
static int  compile_assembly(struct assembly *assembly);
static bool llvm_initialized = false;

static void unit_job(struct job_context *ctx) {
	bassert(ctx);
	compile_unit(ctx->unit, ctx->assembly);
}

static void submit_unit(struct assembly *assembly, struct unit *unit) {
	bassert(unit);
	bassert(builder.options->no_jobs == false);

	struct job_context ctx = {
	    .assembly = assembly,
	    .unit     = unit,
	};
	submit_job(&unit_job, &ctx);
}

static void llvm_init(void) {
	if (llvm_initialized) return;

	LLVMInitializeX86Target();
	LLVMInitializeX86TargetInfo();
	LLVMInitializeX86TargetMC();
	LLVMInitializeX86AsmPrinter();

	LLVMInitializeAArch64Target();
	LLVMInitializeAArch64TargetInfo();
	LLVMInitializeAArch64TargetMC();
	LLVMInitializeAArch64AsmPrinter();

	bassert(LLVMIsMultithreaded() &&
	        "LLVM must be compiled in multi-thread mode with flag 'LLVM_ENABLE_THREADS'");

	llvm_initialized = true;
}

static void llvm_terminate(void) {
	LLVMShutdown();
}

int compile_unit(struct unit *unit, struct assembly *assembly) {
	array(unit_stage_fn_t) pipeline = assembly->current_pipelines.unit;
	bassert(pipeline && "Invalid unit pipeline!");
	if (unit->loaded_from) {
		builder_log(
		    "Compile: %s (loaded from '%s')", unit->name, unit->loaded_from->location.unit->name);
	} else {
		builder_log("Compile: %s", unit->name);
	}
	for (usize i = 0; i < arrlenu(pipeline); ++i) {
		pipeline[i](assembly, unit);
		if (builder.errorc) return COMPILE_FAIL;
	}
	return COMPILE_OK;
}

int compile_assembly(struct assembly *assembly) {
	bassert(assembly);
	array(assembly_stage_fn_t) pipeline = assembly->current_pipelines.assembly;
	bassert(pipeline && "Invalid assembly pipeline!");

	for (usize i = 0; i < arrlenu(pipeline); ++i) {
		if (builder.errorc) return COMPILE_FAIL;
		pipeline[i](assembly);
	}
	return COMPILE_OK;
}

static void entry_run(struct assembly *assembly) {
	vm_entry_run(assembly);
	builder.last_script_mode_run_status = assembly->vm_run.last_execution_status;
}

static void build_entry_run(struct assembly *assembly) {
	vm_build_entry_run(assembly);
}

static void tests_run(struct assembly *assembly) {
	vm_tests_run(assembly);
	builder.test_failc = assembly->vm_run.last_execution_status;
}

static void attach_dbg(struct assembly *assembly) {
	vmdbg_attach(&assembly->vm);
}

static void detach_dbg(struct assembly *assembly) {
	vmdbg_detach();
}

static void setup_unit_pipeline(struct assembly *assembly) {
	bassert(assembly->current_pipelines.unit == NULL);
	array(unit_stage_fn_t) *stages = &assembly->current_pipelines.unit;
	arrsetcap(*stages, 16);

	const struct target *t = assembly->target;
	arrput(*stages, &file_loader_run);
	arrput(*stages, &lexer_run);
	if (t->print_tokens) arrput(*stages, &token_printer_run);
	arrput(*stages, &parser_run);
}

static void setup_assembly_pipeline(struct assembly *assembly) {
	const struct target *t = assembly->target;

	bassert(assembly->current_pipelines.assembly == NULL);
	array(assembly_stage_fn_t) *stages = &assembly->current_pipelines.assembly;
	arrsetcap(*stages, 16);

	if (t->print_ast) arrput(*stages, &ast_printer_run);
	if (t->kind == ASSEMBLY_DOCS) {
		arrput(*stages, &docs_run);
		return;
	}
	if (t->syntax_only) return;
	arrput(*stages, &linker_run);
	arrput(*stages, &mir_run);
	if (t->vmdbg_enabled) arrput(*stages, &attach_dbg);
	if (t->run) arrput(*stages, &entry_run);
	if (t->kind == ASSEMBLY_BUILD_PIPELINE) arrput(*stages, build_entry_run);
	if (t->run_tests) arrput(*stages, tests_run);
	if (t->vmdbg_enabled) arrput(*stages, &detach_dbg);
	if (t->emit_mir) arrput(*stages, &mir_writer_run);
	if (t->no_analyze) return;
	if (t->no_llvm) return;
	if (t->kind == ASSEMBLY_BUILD_PIPELINE) return;

	arrput(*stages, &ir_run);
	arrput(*stages, &ir_opt_run);
	if (t->emit_llvm) arrput(*stages, &bc_writer_run);
	if (t->emit_asm) arrput(*stages, &asm_writer_run);
	if (t->no_bin) return;
	arrput(*stages, &obj_writer_run);

	arrput(*stages, &native_bin_run);
}

static void print_stats(struct assembly *assembly) {

	const f64 total_s = assembly->stats.parsing_lexing_s + assembly->stats.mir_s +
	                    assembly->stats.llvm_s + assembly->stats.linking_s +
	                    assembly->stats.llvm_obj_s;

	builder_info(
	    "--------------------------------------------------------------------------------\n"
	    "Compilation stats for '%s'\n"
	    "--------------------------------------------------------------------------------\n"
	    "Time:\n"
	    "  Lexing & Parsing: %10.3f seconds    %3.0f%%\n"
	    "  MIR:              %10.3f seconds    %3.0f%%\n"
	    "  LLVM IR:          %10.3f seconds    %3.0f%%\n"
	    "  LLVM Obj:         %10.3f seconds    %3.0f%%\n"
	    "  Linking:          %10.3f seconds    %3.0f%%\n\n"
	    "  Polymorph:        %10lld generated in %.3f seconds\n\n"
	    "  Total:            %10.3f seconds\n"
	    "  Lines:              %8d\n"
	    "  Speed:            %10.0f lines/second\n\n"
	    "MISC:\n"
	    "  Allocated stack snapshot count: %lld\n",
	    assembly->target->name,
	    assembly->stats.parsing_lexing_s,
	    assembly->stats.parsing_lexing_s / total_s * 100.,
	    assembly->stats.mir_s,
	    assembly->stats.mir_s / total_s * 100.,
	    assembly->stats.llvm_s,
	    assembly->stats.llvm_s / total_s * 100.,
	    assembly->stats.llvm_obj_s,
	    assembly->stats.llvm_obj_s / total_s * 100.,
	    assembly->stats.linking_s,
	    assembly->stats.linking_s / total_s * 100.,
	    assembly->stats.polymorph_count,
	    assembly->stats.polymorph_s,
	    total_s,
	    builder.total_lines,
	    ((f64)builder.total_lines) / total_s,
	    assembly->stats.comptime_call_stacks_count);
}

static void clear_stats(struct assembly *assembly) {
	memset(&assembly->stats, 0, sizeof(assembly->stats));
}

static int compile(struct assembly *assembly) {
	s32 state           = COMPILE_OK;
	builder.total_lines = 0;
	builder.errorc      = 0;
	builder.auto_submit = false;

	setup_unit_pipeline(assembly);
	setup_assembly_pipeline(assembly);

	runtime_measure_begin(process_unit);

	if (builder.options->no_jobs) {
		blog("Running in single thread mode!");
		for (usize i = 0; i < arrlenu(assembly->units); ++i) {
			struct unit *unit = assembly->units[i];
			if ((state = compile_unit(unit, assembly)) != COMPILE_OK) break;
		}
	} else {
		builder.auto_submit = true;

		// Compile all available units in perallel and wait for all threads to finish...
		for (usize i = 0; i < arrlenu(assembly->units); ++i) {
			submit_unit(assembly, assembly->units[i]);
		}
		wait_threads();

		builder.auto_submit = false;
	}

	assembly->stats.parsing_lexing_s = runtime_measure_end(process_unit);

	// Compile assembly using pipeline.
	if (state == COMPILE_OK) state = compile_assembly(assembly);

	arrfree(assembly->current_pipelines.unit);
	assembly->current_pipelines.unit = NULL;

	arrfree(assembly->current_pipelines.assembly);
	assembly->current_pipelines.assembly = NULL;

	if (state != COMPILE_OK) {
		if (assembly->target->kind == ASSEMBLY_BUILD_PIPELINE) {
			builder_error("Build pipeline failed.");
		} else {
			builder_error("Compilation of target '%s' failed.", assembly->target->name);
		}
	}
	if (builder.options->stats && assembly->target->kind != ASSEMBLY_BUILD_PIPELINE) {
		print_stats(assembly);
	}
	clear_stats(assembly);

	if (builder.errorc) return builder.max_error;
	if (assembly->target->run) return builder.last_script_mode_run_status;
	if (assembly->target->run_tests) return builder.test_failc;
	return EXIT_SUCCESS;
}

// =================================================================================================
// PUBLIC
// =================================================================================================

void builder_init(struct builder_options *options, const char *exec_dir) {
	bassert(options && "Invalid builder options!");
	bassert(exec_dir && "Invalid executable directory!");
	memset(&builder, 0, sizeof(struct builder));
	builder.options = options;
	builder.errorc = builder.max_error = builder.test_failc = 0;
	builder.last_script_mode_run_status                     = 0;

	builder.exec_dir = strdup(exec_dir);

	// initialize LLVM statics
	llvm_init();
	// Generate hashes for builtin ids.
	for (s32 i = 0; i < _BUILTIN_ID_COUNT; ++i) {
		builtin_ids[i].hash = strhash(builtin_ids[i].str);
	}

	builder.sync = sync_new();

	init_thread_local_storage();
	start_threads(cpu_thread_count());

	builder.is_initialized = true;
}

void builder_terminate(void) {
	stop_threads();
	terminate_thread_local_storage();

	sync_delete(builder.sync);

	for (usize i = 0; i < arrlenu(builder.targets); ++i) {
		target_delete(builder.targets[i]);
	}
	arrfree(builder.targets);

	confdelete(builder.config);
	llvm_terminate();
	free(builder.exec_dir);
}

char **builder_get_supported_targets(void) {
	const bool ex = builder.options->enable_experimental_targets;

	const usize l1  = static_arrlenu(supported_targets);
	const usize l2  = static_arrlenu(supported_targets_experimental);
	const usize len = ex ? (l1 + l2 + 1) : l1 + 1; // +1 for terminator

	char **dest = bmalloc(len * sizeof(char *));
	memcpy(dest, supported_targets, l1 * sizeof(char *));
	if (ex) {
		memcpy(dest + l1, supported_targets_experimental, l2 * sizeof(char *));
	}
	dest[len - 1] = NULL;
	return dest;
}

const char *builder_get_lib_dir(void) {
	return confreads(builder.config, CONF_LIB_DIR_KEY, NULL);
}

const char *builder_get_exec_dir(void) {
	bassert(builder.exec_dir && "Executable directory not set, call 'builder_init' first.");
	return builder.exec_dir;
}

bool builder_load_config(const str_t filepath) {
	confdelete(builder.config);
	builder.config = confload(str_to_c(filepath));
	return (bool)builder.config;
}

struct target *builder_add_target(const char *name) {
	bassert(builder.default_target && "Default target must be set first!");
	struct target *target = target_dup(name, builder.default_target);
	bassert(target);
	arrput(builder.targets, target);
	return target;
}

struct target *builder_add_default_target(const char *name) {
	bassert(!builder.default_target && "Default target is already set!");
	struct target *target = target_new(name);
	bassert(target);
	builder.default_target = target;
	arrput(builder.targets, target);
	return target;
}

struct target *_builder_add_target(const char *name, bool is_default) {
	struct target *target = NULL;
	if (is_default) {
		target                 = target_new(name);
		builder.default_target = target;
	} else {
		target = target_dup(name, builder.default_target);
	}
	bassert(target);
	arrput(builder.targets, target);
	return target;
}

int builder_compile_all(void) {
	s32 state = COMPILE_OK;
	for (usize i = 0; i < arrlenu(builder.targets); ++i) {
		struct target *target = builder.targets[i];
		if (target->kind == ASSEMBLY_BUILD_PIPELINE) continue;
		state = builder_compile(target);
		if (state != COMPILE_OK) break;
	}
	return state;
}

s32 builder_compile(const struct target *target) {
	bmagic_assert(target);
	struct assembly *assembly = assembly_new(target);

	s32 state = compile(assembly);

	if (builder.options->do_cleanup_when_done) {
		assembly_delete(assembly);
	}
	return state;
}

void builder_print_location(FILE *stream, struct location *loc, s32 col, s32 len) {
	long      line_len = 0;
	const s32 padding  = snprintf(NULL, 0, "%+d", loc->line) + 2;
	// Line one
	const char *line_str = unit_get_src_ln(loc->unit, loc->line - 1, &line_len);
	if (line_str && line_len) {
		fprintf(stream, "\n%*d | %.*s", padding, loc->line - 1, (int)line_len, line_str);
	}
	// Line two
	line_str = unit_get_src_ln(loc->unit, loc->line, &line_len);
	if (line_str && line_len) {
		color_print(
		    stream, BL_YELLOW, "\n>%*d | %.*s", padding - 1, loc->line, (int)line_len, line_str);
	}
	// Line cursors
	if (len > 0) {
		char buf[256];
		s32  written_bytes = 0;
		for (s32 i = 0; i < col + len - 1; ++i) {
			// We need to do this to properly handle tab indentation.
			char insert = line_str[i];
			if (insert != ' ' && insert != '\t') insert = ' ';
			if (i >= col - 1) insert = '^';

			written_bytes +=
			    snprintf(buf + written_bytes, static_arrlenu(buf) - written_bytes, "%c", insert);
		}
		fprintf(stream, "\n%*s | ", padding, "");
		color_print(stream, BL_GREEN, "%s", buf);
	}

	// Line three
	line_str = unit_get_src_ln(loc->unit, loc->line + 1, &line_len);
	if (line_str && line_len) {
		fprintf(stream, "\n%*d | %.*s", padding, loc->line + 1, (int)line_len, line_str);
	}
	fprintf(stream, "\n\n");
}

static inline bool should_report(enum builder_msg_type type) {
	const struct builder_options *opt = builder.options;
	switch (type) {
	case MSG_LOG:
		return opt->verbose && !opt->silent;
	case MSG_INFO:
		return !opt->silent;
	case MSG_WARN:
		return !opt->no_warning && !opt->silent;
	case MSG_ERR_NOTE:
	case MSG_ERR:
		return builder.errorc < opt->error_limit;
	}
	babort("Unknown message type!");
}

void builder_vmsg(enum builder_msg_type type,
                  s32                   code,
                  struct location      *src,
                  enum builder_cur_pos  pos,
                  const char           *format,
                  va_list               args) {
	pthread_mutex_lock(&builder.sync->log_mutex);
	if (!should_report(type)) goto DONE;

	FILE *stream = stdout;
	if (type == MSG_ERR || type == MSG_ERR_NOTE) {
		stream = stderr;
		builder.errorc++;
		builder.max_error = code > builder.max_error ? code : builder.max_error;
	}

	if (src) {
		const char *filepath =
		    builder.options->full_path_reports ? src->unit->filepath : src->unit->filename;
		s32 line = src->line;
		s32 col  = src->col;
		s32 len  = src->len;
		switch (pos) {
		case CARET_AFTER:
			col += len;
			len = 1;
			break;
		case CARET_BEFORE:
			col -= col < 1 ? 0 : 1;
			len = 1;
			break;
		case CARET_NONE:
			len = 0;
			break;
		default:
			break;
		}
		fprintf(stream, "%s:%d:%d: ", filepath, line, col);
		switch (type) {
		case MSG_ERR: {
			if (code > NO_ERR)
				color_print(stream, BL_RED, "error(%04d): ", code);
			else
				color_print(stream, BL_RED, "error: ");
			break;
		}
		case MSG_WARN: {
			color_print(stream, BL_YELLOW, "warning: ");
			break;
		}

		default:
			break;
		}
		vfprintf(stream, format, args);
		builder_print_location(stream, src, col, len);
	} else {
		switch (type) {
		case MSG_ERR: {
			if (code > NO_ERR)
				color_print(stream, BL_RED, "error(%04d): ", code);
			else
				color_print(stream, BL_RED, "error: ");
			break;
		}
		case MSG_WARN: {
			color_print(stream, BL_YELLOW, "warning: ");
			break;
		}
		default:
			break;
		}
		vfprintf(stream, format, args);
		fprintf(stream, "\n");
	}
DONE:
	pthread_mutex_unlock(&builder.sync->log_mutex);

#if ASSERT_ON_CMP_ERROR
	if (type == MSG_ERR) bassert(false);
#endif
}

void builder_msg(enum builder_msg_type type,
                 s32                   code,
                 struct location      *src,
                 enum builder_cur_pos  pos,
                 const char           *format,
                 ...) {
	va_list args;
	va_start(args, format);
	builder_vmsg(type, code, src, pos, format, args);
	va_end(args);
}

str_buf_t get_tmp_str(void) {
	zone();
	str_buf_t str = {0};

	struct thread_local_storage *storage = get_thread_local_storage();
	if (arrlenu(storage->temporary_strings)) {
		str = arrpop(storage->temporary_strings);
	} else {
		str_buf_setcap(&str, 255);
	}
	str_buf_clr(&str); // also set zero terminator
	return_zone(str);
}

void put_tmp_str(str_buf_t str) {
	struct thread_local_storage *storage = get_thread_local_storage();
	arrput(storage->temporary_strings, str);
}

void builder_async_submit_unit(struct assembly *assembly, struct unit *unit) {
	bassert(unit);
	bassert(builder.options->no_jobs == false);
	if (builder.auto_submit) {
		submit_unit(assembly, unit);
	}
}
