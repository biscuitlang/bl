// =================================================================================================
// bl
//
// File:   build_api.c
// Author: Martin Dorazil
// Date:   8/1/20
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

// Those methods are used only during compilation pass to setup builder pipeline. No C interfaces
// are needed here.

#include "builder.h"
#include "stb_ds.h"

BL_EXPORT struct target *__add_target(const char *name, enum assembly_kind kind) {
	struct target *target = builder_add_target(name);
	target->kind          = kind;
	return target;
}

BL_EXPORT void __add_unit(struct target *target, const char *filepath) {
	target_add_file(target, filepath);
}

BL_EXPORT void __add_lib_path(struct target *target, const char *path) {
	target_add_lib_path(target, path);
}

BL_EXPORT s32 __compile(struct target *target) {
	return builder_compile(target);
}

BL_EXPORT s32 __compile_all(void) {
	return builder_compile_all();
}

BL_EXPORT void __link_library(struct target *target, const char *name) {
	target_add_lib(target, name);
}

BL_EXPORT void __append_linker_options(struct target *target, const char *opt) {
	target_append_linker_options(target, opt);
}

BL_EXPORT void __set_output_dir(struct target *target, const char *dir) {
	target_set_output_dir(target, dir);
}

BL_EXPORT const char *__get_output_dir(struct target *target) {
	bmagic_assert(target);
	return target->out_dir.len > 0 ? str_buf_to_c(target->out_dir) : NULL;
}

BL_EXPORT void __set_module_dir(struct target *target, const char *dir) {
	target_set_module_dir(target, dir);
}

BL_EXPORT const char *__get_module_dir(struct target *target) {
	bmagic_assert(target);
	return target->module_dir.len > 0 ? str_buf_to_c(target->module_dir) : NULL;
}

BL_EXPORT void __get_default_triple(struct target_triple *triple) {
	target_init_default_triple(triple);
}

BL_EXPORT s32 __triple_to_string(struct target_triple *triple, char *buf, s32 buf_len) {
	return target_triple_to_string(triple, buf, buf_len);
}

BL_EXPORT void __builder_get_options(struct builder_options *opt) {
	memcpy(opt, builder.options, sizeof(struct builder_options));
}

BL_EXPORT void __builder_set_options(struct builder_options *opt) {
	memcpy(builder.options, opt, sizeof(struct builder_options));
}

// @Incomplete 2024-12-16: We miss API to import modules!!!!