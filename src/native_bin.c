// =================================================================================================
// bl
//
// File:   native_bin.c
// Author: Martin Dorazil
// Date:   10/03/2018
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
#include "stb_ds.h"

#if !BL_PLATFORM_WIN
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

typedef s32 (*LinkerFn)(struct assembly *);

s32 lld_link(struct assembly *assembly);
s32 lld_ld(struct assembly *assembly);

static void copy_user_libs(struct assembly *assembly) {
	str_buf_t            dest_path = get_tmp_str();
	const struct target *target    = assembly->target;
	const str_t          out_dir   = str_buf_view(target->out_dir);
	for (usize i = 0; i < arrlenu(assembly->libs); ++i) {
		struct native_lib *lib = &assembly->libs[i];
		if (lib->is_internal) continue;
		if (!lib->user_name.len) continue;

		str_buf_clr(&dest_path);
		str_t lib_dest_name = lib->filename;
#if BL_PLATFORM_LINUX || BL_PLATFORM_MACOS
		struct stat statbuf;
		str_buf_t tmp_filepath = get_tmp_str();
		const char *clib_filepath = str_to_c(&tmp_filepath, lib->filepath);
		lstat(clib_filepath, &statbuf);
		if (S_ISLNK(statbuf.st_mode)) {
			char buf[PATH_MAX] = {0};
			if (readlink(clib_filepath, buf, static_arrlenu(buf)) == -1) {
				builder_error("Cannot follow symlink '%.*s' with error: %d",
				              lib->filepath.len,
				              lib->filepath.ptr,
				              errno);
				put_tmp_str(tmp_filepath);
				continue;
			}
			lib_dest_name = make_str_from_c(buf);
		}
		put_tmp_str(tmp_filepath);
#endif

		str_buf_append_fmt(&dest_path, "{str}/{str}", out_dir, lib_dest_name);
		if (file_exists(dest_path)) continue;

		builder_info("Copy '%.*s' to '%.*s'.",
		             lib->filepath.len,
		             lib->filepath.ptr,
		             dest_path.len,
		             dest_path.ptr);

		if (!copy_file(lib->filepath, str_buf_view(dest_path))) {
			builder_error("Cannot copy '%.*s' to '%.*s'.",
			              lib->filepath.len,
			              lib->filepath.ptr,
			              dest_path.len,
			              dest_path.ptr);
		}
	}
	put_tmp_str(dest_path);
}

void native_bin_run(struct assembly *assembly) {
	builder_log("Running native runtime linker...");
	LinkerFn linker = NULL;
#if BL_PLATFORM_WIN
	linker = &lld_link;
#elif BL_PLATFORM_LINUX || BL_PLATFORM_MACOS
	linker = &lld_ld;
#else
#error "Unknown platform"
#endif

	const str_t out_dir = str_buf_view(assembly->target->out_dir);
	zone();
	if (linker(assembly) != 0) {
		builder_msg(MSG_ERR, ERR_LIB_NOT_FOUND, NULL, CARET_WORD, "Native link execution failed.");
		goto DONE;
	}

	if (assembly->target->copy_deps) {
		builder_log("Copy assembly dependencies into '%.*s'.", out_dir.len, out_dir.ptr);
		copy_user_libs(assembly);
	}
DONE:
	return_zone();
}
