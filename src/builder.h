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

#ifndef BL_BUILDER_H
#define BL_BUILDER_H

#include "assembly.h"

#define COMPILE_OK 0
#define COMPILE_FAIL 1

struct config;

// Keep in sync with build.bl API!!!
struct builder_options {
	bool verbose;
	bool no_color;
	bool silent;
	bool no_jobs;
	bool no_warning;
	bool full_path_reports;
	bool no_usage_check;
	bool stats;
	bool enable_experimental_targets;
	bool do_cleanup_when_done;
	s32  error_limit;

	char *doc_out_dir;
};

struct builder {
	struct builder_options *options;
	const struct target    *default_target;
	str_buf_t               exec_dir;
	batomic_s32             total_lines;
	s32                     errorc;
	s32                     max_error;
	s32                     test_failc;
	s32                     last_script_mode_run_status;
	struct config          *config;
	array(struct target *) targets;

	struct assembly *current_executed_assembly;

	// Used for multithreaded compiling, in case new unit is added while parsing,
	// new job for it is submitted.
	bool  auto_submit;
	mtx_t log_mutex;
	bool  is_initialized;
};

// struct builder global instance.
extern struct builder builder;

enum builder_msg_type {
	MSG_LOG = 0,
	MSG_INFO,
	MSG_WARN,
	MSG_ERR_NOTE,
	MSG_ERR,
};

enum builder_cur_pos { CARET_WORD = 0,
	                   CARET_BEFORE,
	                   CARET_AFTER,
	                   CARET_NONE };

struct location;

void builder_init(struct builder_options *options);
void builder_terminate(void);
// Return zero terminated list of supported target triples. Must be disposed by bfree.
char         **builder_get_supported_targets(void);
const char    *builder_get_lib_dir(void);
const str_t    builder_get_exec_dir(void);
bool           builder_load_config(const str_t filepath);
struct target *builder_add_target(const char *name);
struct target *builder_add_default_target(const char *name);
s32            builder_compile_all(void);
s32            builder_compile(const struct target *target);

// Submit new unit for async compilation, in case no-jobs flag is set, this function does nothing.
void builder_submit_unit(struct assembly *assembly, struct unit *unit);

#define builder_log(format, ...) builder_msg(MSG_LOG, -1, NULL, CARET_NONE, format, ##__VA_ARGS__)
#define builder_info(format, ...) builder_msg(MSG_INFO, -1, NULL, CARET_NONE, format, ##__VA_ARGS__)
#define builder_note(format, ...) builder_msg(MSG_ERR_NOTE, -1, NULL, CARET_NONE, format, ##__VA_ARGS__)
#define builder_warning(format, ...) builder_msg(MSG_WARN, -1, NULL, CARET_NONE, format, ##__VA_ARGS__)
#define builder_error(format, ...) builder_msg(MSG_ERR, -1, NULL, CARET_NONE, format, ##__VA_ARGS__)

void builder_vmsg(enum builder_msg_type type,
                  s32                   code,
                  struct location      *src,
                  enum builder_cur_pos  pos,
                  const char           *format,
                  va_list               args);

void builder_msg(enum builder_msg_type type,
                 s32                   code,
                 struct location      *src,
                 enum builder_cur_pos  pos,
                 const char           *format,
                 ...);

// Temporary strings.
str_buf_t get_tmp_str(void);
void      put_tmp_str(str_buf_t str);

inline static str_buf_t get_tmp_str_from(const str_t s) {
	str_buf_t tmp = get_tmp_str();
	str_buf_append(&tmp, s);
	return tmp;
}

void builder_print_location(FILE *stream, struct location *loc, s32 col, s32 len);

#endif
