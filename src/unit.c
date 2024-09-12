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
#include "stb_ds.h"
#include <string.h>

#if BL_PLATFORM_WIN
#include <windows.h>
#endif

#define EXPECTED_ARRAY_COUNT 64

hash_t unit_hash(const char *filepath, struct token *load_from) {
	struct unit *parent_unit = load_from ? load_from->location.unit : NULL;
	char        *real_path   = NULL;
	search_source_file(
	    filepath, SEARCH_FLAG_ALL, parent_unit ? parent_unit->dirpath : NULL, &real_path, NULL);
	const char  *path = real_path ? real_path : filepath;
	const hash_t hash = strhash(make_str_from_c(path));
	free(real_path);
	return hash;
}

// public
struct unit *unit_new(const char *filepath, struct token *load_from) {
	struct unit *parent_unit = load_from ? load_from->location.unit : NULL;
	struct unit *unit        = bmalloc(sizeof(struct unit));
	bl_zeromem(unit, sizeof(struct unit));

	search_source_file(filepath,
	                   SEARCH_FLAG_ALL,
	                   parent_unit ? parent_unit->dirpath : NULL,
	                   &unit->filepath,
	                   &unit->dirpath);
	unit->name         = strdup(filepath);
	char tmp[PATH_MAX] = {0};
	if (get_filename_from_filepath(tmp, static_arrlenu(tmp), filepath)) {
		unit->filename = strdup(tmp);
	} else {
		babort("invalid file");
	}
	unit->loaded_from = load_from;
	const char *path  = unit->filepath ? unit->filepath : unit->name;
	unit->hash        = strhash(make_str_from_c(path));
	tokens_init(&unit->tokens);
	return unit;
}

void unit_delete(struct unit *unit) {
	arrfree(unit->ublock_ast);
	str_buf_free(&unit->file_docs_cache);
	bfree(unit->src);
	free(unit->filepath);
	free(unit->dirpath);
	free(unit->name);
	free(unit->filename);
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
