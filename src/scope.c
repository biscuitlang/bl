// =================================================================================================
// bl
//
// File:   scope.c
// Author: Martin Dorazil
// Date:   3/14/18
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

#include "scope.h"
#include "assembly.h"
#include "stb_ds.h"
#include "table.h"
#include "threading.h"
#include "unit.h"

BL_STATIC_ASSERT(sizeof(BL_TBL_HASH_T) == sizeof(u64), "Scope require hash value to be 64bit.");

#define entry_hash(id, layer) ((((u64)layer) << 32) | (u64)id)

static void scope_dtor(struct scope *scope) {
	bmagic_assert(scope);
	tbl_free(scope->entries);
	arrfree(scope->injected);
	mtx_destroy(&scope->lock);
}

void scope_thread_local_init(struct scope_thread_local *local, u32 owner_thread_index) {
	arena_init(&local->scopes, sizeof(struct scope), alignment_of(struct scope), 256, owner_thread_index, (arena_elem_dtor_t)scope_dtor);
	arena_init(&local->entries, sizeof(struct scope_entry), alignment_of(struct scope_entry), 8192, owner_thread_index, NULL);
	local->lookup_queue = NULL;
}

void scope_thread_local_terminate(struct scope_thread_local *local) {
	arena_terminate(&local->scopes);
	arena_terminate(&local->entries);
	tbl_free(local->lookup_queue);
}

struct scope *scope_create(struct scope_thread_local *local,
                           enum scope_kind            kind,
                           struct scope              *parent,
                           struct location           *loc) {
	bassert(kind != SCOPE_NONE && "Invalid scope kind.");
	struct scope *scope = arena_alloc(&local->scopes);
	scope->parent       = parent;
	scope->kind         = kind;
	scope->location     = loc;

	mtx_init(&scope->lock, mtx_recursive);

	bmagic_set(scope);
	return scope;
}

void scope_reserve(struct scope *scope, s32 num) {
	bassert(scope->entries == NULL && "This can be called only once before the first use!");
	tbl_init(scope->entries, num);
}

struct scope_entry *scope_create_entry(struct scope_thread_local *local,
                                       enum scope_entry_kind      kind,
                                       struct id                 *id,
                                       struct ast                *node,
                                       bool                       is_builtin) {
	struct scope_entry *entry = arena_alloc(&local->entries);
	entry->id                 = id;
	entry->kind               = kind;
	entry->node               = node;
	entry->is_builtin         = is_builtin;
	entry->ref_count          = 0;
	bmagic_set(entry);
	return entry;
}

void scope_insert(struct scope *scope, hash_t layer, struct scope_entry *entry) {
	zone();
	bassert(scope);
	bassert(entry && entry->id);
	const u64 hash = entry_hash(entry->id->hash, layer);
	bassert(tbl_lookup_index_with_key(scope->entries, hash, entry->id->str) == -1 && "Duplicate scope entry key!!!");
	entry->parent_scope              = scope;
	struct scope_tbl_entry tbl_entry = {
	    .hash  = hash,
	    .key   = entry->id->str,
	    .value = entry,
	};
	tbl_insert(scope->entries, tbl_entry);
	return_zone();
}

struct search_context {
	hash_table(struct scope_lookup_queue_entry) * queue;
	u32 queue_index;

	s32                  found_num;
	s32                  found_buf_size;
	struct scope_entry **found_buf;
};

static void search_scope(struct search_context *ctx, struct scope *root_scope, scope_lookup_args_t *args) {
	if (tbl_lookup_index(*ctx->queue, root_scope) != -1) return; // Scope already processed...

	u32 layer = SCOPE_DEFAULT_LAYER;
	if (scope_is_local(root_scope)) {
		layer = args->layer;
	} else if (args->local_only) {
		return;
	}

	tbl_insert(*ctx->queue, (struct scope_lookup_queue_entry){.hash = root_scope});
	for (; ctx->queue_index < tbl_len(*ctx->queue) && ctx->found_num < ctx->found_buf_size; ++ctx->queue_index) {
		struct scope *scope = (*ctx->queue)[ctx->queue_index].hash;
		bassert(scope->kind != SCOPE_NONE);
		const u64 hash = entry_hash(args->id->hash, layer);
		layer          = SCOPE_DEFAULT_LAYER;

		const bool is_locked = scope->kind == SCOPE_GLOBAL || scope->kind == SCOPE_MODULE || scope->kind == SCOPE_MODULE_PRIVATE;
		if (is_locked) scope_lock(scope);

		const s64 index = tbl_lookup_index_with_key(scope->entries, hash, args->id->str);
		if (index != -1) ctx->found_buf[ctx->found_num++] = scope->entries[index].value;

		for (usize injected_index = 0; injected_index < arrlenu(scope->injected); ++injected_index) {
			struct scope *injected_scope = scope->injected[injected_index];
			if (tbl_lookup_index(*ctx->queue, injected_scope) != -1) continue;
			tbl_insert(*ctx->queue, (struct scope_lookup_queue_entry){.hash = injected_scope});
		}

		if (is_locked) scope_unlock(scope);
	}
}

s32 scope_lookup(struct assembly *assembly, struct scope *scope, scope_lookup_args_t *args, struct scope_entry **out_buf, const s32 out_buf_size) {
	zone();
	bassert(scope && args->id);
	const u32 thread_index = get_worker_index();

	struct search_context ctx = {
	    .queue = &assembly->thread_local_contexts[thread_index].scope_thread_local.lookup_queue,

	    .found_buf      = out_buf,
	    .found_buf_size = out_buf_size,
	};

	tbl_clear(*ctx.queue);

	while (scope && ctx.found_num < ctx.found_buf_size) {
		search_scope(&ctx, scope, args);

		if (ctx.found_num && scope->kind != SCOPE_MODULE_PRIVATE) break;
		if (!args->in_tree) break;
		if (args->out_of_function) *(args->out_of_function) = scope->kind == SCOPE_FN;

		scope = scope->parent;
	}

	return_zone(ctx.found_num);
}

void scope_lock(struct scope *scope) {
	mtx_lock(&scope->lock);
}

void scope_unlock(struct scope *scope) {
	mtx_unlock(&scope->lock);
}

void scope_inject(struct scope *dest, struct scope *src) {
	bmagic_assert(dest);
	bmagic_assert(src);
	bassert(dest != src && "Injecting scope to itself!");
	// bassert(src->kind == SCOPE_MODULE);
	// bassert(!scope_is_local(dest) && "Injection destination scope must be global!");
	const bool is_locked = dest->kind == SCOPE_GLOBAL || dest->kind == SCOPE_MODULE;
	if (is_locked) scope_lock(dest);
	for (usize i = 0; i < arrlenu(dest->injected); ++i) {
		if (src == dest->injected[i]) {
			if (is_locked) scope_unlock(dest);
			return;
		}
	}
	if (arrlen(dest->injected) == 0) arrsetcap(dest->injected, 8); // Preallocate a bit...
	arrput(dest->injected, src);
	if (is_locked) scope_unlock(dest);
}

bool scope_is_subtree_of_kind(const struct scope *scope, enum scope_kind kind) {
	while (scope) {
		if (scope->kind == kind) return true;
		scope = scope->parent;
	}
	return false;
}

bool scope_is_subtree_of(const struct scope *scope, const struct scope *other) {
	while (scope) {
		if (scope == other) return true;
		scope = scope->parent;
	}
	return false;
}

struct scope *scope_find_closest_global(struct scope *scope) {
	while (scope) {
		if (!scope_is_local(scope)) break;
		scope = scope->parent;
	}
	return scope;
}

const char *scope_kind_name(const struct scope *scope) {
	if (!scope) return "<INVALID>";
	switch (scope->kind) {
	case SCOPE_NONE:
		return "None";
	case SCOPE_GLOBAL:
		return "Global";
	case SCOPE_PRIVATE:
		return "Private";
	case SCOPE_FN:
		return "Function";
	case SCOPE_FN_BODY:
		return "Function";
	case SCOPE_LEXICAL:
		return "Lexical";
	case SCOPE_TYPE_STRUCT:
		return "Struct";
	case SCOPE_TYPE_ENUM:
		return "Enum";
	case SCOPE_MODULE:
		return "Module";
	case SCOPE_MODULE_PRIVATE:
		return "ModulePrivate";
	}

	return "<INVALID>";
}

void scope_get_full_name(str_buf_t *buf, struct scope *scope) {
	bassert(scope && buf);
	sarr_t(str_t, 8) tmp = SARR_ZERO;
	while (scope) {
		if (scope->name.len) sarrput(&tmp, scope->name);
		scope = scope->parent;
	}

	str_t dot = cstr(".");
	for (usize i = sarrlenu(&tmp); i-- > 0;) {
		const str_t subname = sarrpeek(&tmp, i);
		// str_buf_append(buf, dot);
		str_buf_append(buf, subname);
		if (i > 0) {
			str_buf_append(buf, dot);
		}
	}
	sarrfree(&tmp);
}
