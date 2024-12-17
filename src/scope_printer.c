// =================================================================================================
// bl
//
// File:   scope_printer.c
// Author: Martin Dorazil
// Date:   12/17/24
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

#include "assembly.h"
#include "builder.h"
#include "common.h"
#include "stb_ds.h"
#include "table.h"
#include "threading.h"

#define PRINT_MAX_SYMBOLS 16

struct lookup_entry {
	struct scope *hash;
	s32           id;
};

hash_table(struct lookup_entry) lookup;

static str_t scope_print_dot_name(struct scope *scope, str_buf_t *buf) {
	buf->len = 0;
	str_buf_append_fmt(buf, "[{s}]", scope_kind_name(scope));
	if (scope->name.len > 0) {
		str_buf_append_fmt(buf, " - {str}", scope->name);
	}
	if (scope->filename.len > 0) {
		str_buf_append_fmt(buf, " - {str}", scope->filename);
	}
	return str_buf_view(*buf);
}

static void print_data(struct scope *scope, FILE *stream, s32 id, str_buf_t *buf) {
	str_t name = scope_print_dot_name(scope, buf);

	fprintf(stream, "S%d [\n", id);
	fprintf(stream, "label=<\n");
	fprintf(stream, "<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\">\n");
	fprintf(stream, "<TR><TD BGCOLOR=\"lightblue\"><B>" STR_FMT "</B></TD></TR>\n", STR_ARG(name));
	for (usize i = 0; i < tbl_len(scope->entries); ++i) {
		if (i > PRINT_MAX_SYMBOLS) {
			fprintf(stream, "<TR><TD ALIGN=\"LEFT\">...</TD></TR>\n");
			break;
		}
		fprintf(stream, "<TR><TD ALIGN=\"LEFT\">" STR_FMT "</TD></TR>\n", STR_ARG(scope->entries[i].key));
	}
	fprintf(stream, "</TABLE>");
	fprintf(stream, ">\n");
	fprintf(stream, "];\n");
}

static void scope_print_dot_parenting(struct scope *scope, FILE *stream) {
	const s64 scope_index  = tbl_lookup_index(lookup, scope);
	const s64 parent_index = tbl_lookup_index(lookup, scope->parent);
	if (scope_index == -1 || parent_index == -1) return;

	fprintf(stream, "S%d -> S%d;\n", lookup[scope_index].id, lookup[parent_index].id);
}

static void print_injection(struct scope *scope, FILE *stream) {
	const s64 scope_index = tbl_lookup_index(lookup, scope);
	if (scope_index == -1) return;

	for (usize i = 0; i < arrlenu(scope->injected); ++i) {
		const s64 child_index = tbl_lookup_index(lookup, scope->injected[i]);
		if (child_index == -1) continue;
		fprintf(stream, "S%d -> S%d;\n", lookup[scope_index].id, lookup[child_index].id);
	}
}

static void print_referencing(struct scope *scope, FILE *stream) {
	const s64 scope_index = tbl_lookup_index(lookup, scope);
	if (scope_index == -1) return;

	for (usize i = 0; i < tbl_len(scope->entries); ++i) {
		struct scope_entry *entry = scope->entries[i].value;
		if (entry->kind == SCOPE_ENTRY_NAMED_SCOPE) {
			const s64 ref_index = tbl_lookup_index(lookup, entry->data.scope);
			if (ref_index == -1) continue;
			fprintf(stream, "S%d -> S%d [label=\"ref\"];\n", lookup[scope_index].id, lookup[ref_index].id);
		}
	}
}

void assembly_dump_scope_structure(struct assembly *assembly, FILE *stream, enum scope_dump_mode mode) {
	array(struct scope *) flatten_scopes = NULL;

	const u32 thread_count = get_thread_count();
	for (u32 i = 0; i < thread_count; ++i) {
		arena_get_flatten(&assembly->thread_local_contexts[i].scope_arenas.scopes, (array(void *) *)&flatten_scopes);
	}

	tbl_init(lookup, 1024);

	fprintf(stream, "digraph Scopes {\n");
	fprintf(stream, "node [shape=plaintext]\n");

	str_buf_t buf = get_tmp_str();
	for (s32 i = 0; i < arrlen(flatten_scopes); ++i) {
		struct scope *scope = flatten_scopes[i];
		switch (scope->kind) {
		case SCOPE_GLOBAL:
		case SCOPE_FILE:
		case SCOPE_NAMED:
		case SCOPE_PRIVATE:
		case SCOPE_MODULE: {
			struct lookup_entry entry = (struct lookup_entry){.hash = scope, .id = i};
			tbl_insert(lookup, entry);
			print_data(scope, stream, i, &buf);
			break;
		}
		default:
			break;
		}
	}

	for (s32 i = 0; i < arrlen(flatten_scopes); ++i) {
		struct scope *scope = flatten_scopes[i];
		switch (scope->kind) {
		case SCOPE_GLOBAL:
		case SCOPE_FILE:
		case SCOPE_NAMED:
		case SCOPE_PRIVATE:
		case SCOPE_MODULE: {
			if (mode == SCOPE_DUMP_MODE_PARENTING) {
				scope_print_dot_parenting(scope, stream);
			} else {
				print_injection(scope, stream);
				print_referencing(scope, stream);
			}
			break;
		}
		default:
			break;
		}
	}

	fprintf(stream, "}\n");

	tbl_free(lookup);
	put_tmp_str(buf);
	arrfree(flatten_scopes);
}
