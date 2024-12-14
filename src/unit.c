// =================================================================================================
// bl
//
// File:   unit.c
// Author: Martin Dorazil
// Date:   26.1.18
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

#include "unit.h"
#include "assembly.h"
#include "builder.h"
#include "stb_ds.h"
#include <string.h>

#if BL_PLATFORM_WIN
#include <windows.h>
#endif

#define EXPECTED_ARRAY_COUNT 64

// public
struct unit *unit_new(struct assembly *assembly, const str_t filepath, const str_t name, const hash_t hash, struct token *load_from, struct scope *inject_to_scope) {
	struct unit *unit = bmalloc(sizeof(struct unit)); // @Performance 2024-09-14 Use arena?
	bl_zeromem(unit, sizeof(struct unit));

	const u32             thread_index = get_worker_index();
	struct string_cache **string_cache = &assembly->thread_local_contexts[thread_index].string_cache;

	str_buf_t tmp = get_tmp_str();

	unit->filepath = scdup2(string_cache, filepath);
	unit->name     = scdup2(string_cache, name);
	unit->dirpath  = get_dir_from_filepath(unit->filepath);
	unit->filename = get_filename_from_filepath(unit->filepath);

	unit->hash        = hash;
	unit->loaded_from = load_from;

	bassert(inject_to_scope &&
	        "Missing inject target scope, in case the unit is loaded as an entry file, global scope should be provided, otherwise parent scope of #load directive is supposed to be used.");
	struct scope_arenas *scope_arenas = &assembly->thread_local_contexts[thread_index].scope_arenas;
	bassert(assembly->gscope);
	unit->file_scope = scope_create(scope_arenas, SCOPE_FILE, assembly->gscope, NULL); // 2024-12-13 do we need location?
#ifdef BL_DEBUG
	unit->file_scope->_debug_name = unit->filename;
#endif
	scope_lock(inject_to_scope);
	scope_inject(inject_to_scope, unit->file_scope);
	scope_unlock(inject_to_scope);

	tokens_init(&unit->tokens);
	put_tmp_str(tmp);
	return unit;
}

void unit_delete(struct unit *unit) {
	arrfree(unit->ublock_ast);
	str_buf_free(&unit->file_docs_cache);
	bfree(unit->src);
	tokens_terminate(&unit->tokens);
	bfree(unit);
}

const char *unit_get_src_ln(struct unit *unit, s32 line, long *len) {
	if (line < 1) return NULL;
	// Iterate from begin of the file looking for a specific line.
	const char *c     = unit->src;
	const char *begin = c;
	while (true) {
		if (*c == '\n') {
			--line;
			if (line == 0) break; // Line found.
			begin = c + 1;
		}
		if (*c == '\0') {
			--line;
			break;
		}
		++c;
	}
	if (line > 0) return NULL; // Line not found.
	if (len) {
		long l = (long)(c - begin);
		if (l && begin[l - 1] == '\r') --l;
		bassert(l >= 0);
		*len = l;
	}
	return begin;
}
