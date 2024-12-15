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
#include "stb_ds.h"
#include "table.h"
#include "threading.h"
#include "unit.h"

BL_STATIC_ASSERT(sizeof(BL_TBL_HASH_T) == sizeof(u64), "Scope require hash value to be 64bit.");

#define entry_hash(id, layer) ((((u64)layer) << 32) | (u64)id)

static void scope_dtor(struct scope *scope) {
	bmagic_assert(scope);
	tbl_free(scope->entries);
	arrfree(scope->usings);
	arrfree(scope->injected);
	mtx_destroy(&scope->lock);
	mtx_destroy(&scope->injection_lock);
}

static inline struct scope_entry *lookup_usings(struct scope *scope, struct id *id, struct scope_entry **out_ambiguous) {
	zone();
	bassert(scope && id && out_ambiguous);
	struct scope_entry *found = NULL;
	for (usize i = 0; i < arrlenu(scope->usings); ++i) {
		struct scope_entry *entry =
		    scope_lookup(scope->usings[i], &(scope_lookup_args_t){.id = id});
		if (!entry) continue;
		if (!found) {
			found = entry;
		} else {
			*out_ambiguous = entry;
			break;
		}
	}
	return_zone(found);
}

void scope_arenas_init(struct scope_arenas *arenas, u32 owner_thread_index) {
	arena_init(&arenas->scopes, sizeof(struct scope), alignment_of(struct scope), 256, owner_thread_index, (arena_elem_dtor_t)scope_dtor);
	arena_init(&arenas->entries, sizeof(struct scope_entry), alignment_of(struct scope_entry), 8192, owner_thread_index, NULL);
}

void scope_arenas_terminate(struct scope_arenas *arenas) {
	arena_terminate(&arenas->scopes);
	arena_terminate(&arenas->entries);
}

struct scope *scope_create(struct scope_arenas *arenas,
                           enum scope_kind      kind,
                           struct scope        *parent,
                           struct location     *loc) {
	bassert(kind != SCOPE_NONE && "Invalid scope kind.");
	struct scope *scope = arena_alloc(&arenas->scopes);
	scope->parent       = parent;
	scope->kind         = kind;
	scope->location     = loc;

	mtx_init(&scope->lock, mtx_recursive);
	mtx_init(&scope->injection_lock, mtx_plain);

	bmagic_set(scope);
	return scope;
}

void scope_reserve(struct scope *scope, s32 num) {
	bassert(scope->entries == NULL && "This can be called only once before the first use!");
	tbl_init(scope->entries, num);
}

struct scope_entry *scope_create_entry(struct scope_arenas  *arenas,
                                       enum scope_entry_kind kind,
                                       struct id            *id,
                                       struct ast           *node,
                                       bool                  is_builtin) {
	struct scope_entry *entry = arena_alloc(&arenas->entries);
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
	if (scope->kind == SCOPE_GLOBAL) {
		bcheck_main_thread();
	}
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

struct scope_entry *scope_lookup(struct scope *scope, scope_lookup_args_t *args) {
	zone();
	bassert(scope && args->id);

	struct scope_entry *found       = NULL;
	struct scope_entry *found_using = NULL;
	struct scope_entry *ambiguous   = NULL;

#define REPORTS 0

#if REPORTS
	static s32 hit   = 0;
	static s32 total = 0;
	total++;
#endif

	u64 hash = entry_hash(args->id->hash, args->layer);
	while (scope) {
		bassert(scope->kind != SCOPE_NONE);
		if (scope->kind == args->stop_on) break;
		if (!scope_is_local(scope)) {
			// Global scopes should not have layers!!!
			hash = entry_hash(args->id->hash, SCOPE_DEFAULT_LAYER);
		}

		// Used scopes are handled in following way:
		// If we found symbol in one of used scopes we can eventually use it as found result,
		// however function local symbols are preferred (and can eventually hide symbols from
		// used scope). This approach does not apply for global symbols; in case we have global
		// with the same name as one of symbols from used scopes we must report an error (symbol
		// is ambiguous). An ambiguous symbol is also symbol found in more than one of used
		// scopes and must be also reported.
		if (!found_using && args->out_ambiguous) {
			found_using = lookup_usings(scope, args->id, &ambiguous);
		}

		scope_lock(scope);
		const s64 index = tbl_lookup_index_with_key(scope->entries, hash, args->id->str);
		if (index != -1) {
			found = scope->entries[index].value;
			bassert(found);
		}
		scope_unlock(scope);

		if (!found) {
			mtx_lock(&scope->injection_lock);
			for (usize injected_index = 0; injected_index < arrlenu(scope->injected) && !found; ++injected_index) {
				struct scope *injected_scope = scope->injected[injected_index];
				bassert(injected_scope->kind == SCOPE_FILE);

				scope_lock(injected_scope);
				const s64 index = tbl_lookup_index_with_key(injected_scope->entries, hash, args->id->str);
				if (index != -1) {
					found = injected_scope->entries[index].value;
					bassert(found);
				}
				scope_unlock(injected_scope);
			}
			mtx_unlock(&scope->injection_lock);
		}

		if (found) {
			if (!scope_is_local(found->parent_scope) && found_using) {
				bassert(args->out_ambiguous);
				(*args->out_ambiguous) = found_using;
			}
			break;
		}

		// Lookup in parent.
		if (!args->in_tree) {
			break;
		}
		if (args->out_of_local) *(args->out_of_local) = scope->kind == SCOPE_FN;
		scope = scope->parent;
	}

	if (!found && args->out_ambiguous) {
		// Maybe we have some result coming from used scopes, and it can also be ambiguous (same
		// inside multiple of used scopes).
		found                  = found_using;
		(*args->out_ambiguous) = ambiguous;
	}
#if REPORTS
	if (found) hit++;
	printf("(%3.0f%%) [%d/%d]\n", ((f32)hit / (f32)total) * 100.f, hit, total);
#endif
	return_zone(found);
}

void scope_lock(struct scope *scope) {
	mtx_lock(&scope->lock);
}

void scope_unlock(struct scope *scope) {
	mtx_unlock(&scope->lock);
}

bool scope_using_add(struct scope *scope, struct scope *other) {
	bmagic_assert(scope);
	bmagic_assert(other);
	for (usize i = 0; i < arrlenu(scope->usings); ++i) {
		if (other == scope->usings[i]) {
			// mtx_unlock(&scope->lock); @Cleanup 2024-12-13 This is probably some leftover???
			return false;
		}
	}
	arrput(scope->usings, other);
	return true;
}

void scope_inject(struct scope *scope, struct scope *other) {
	bmagic_assert(scope);
	bmagic_assert(other);
	bassert(scope != other && "Injecting scope to itself!");
	bassert(other->kind == SCOPE_FILE);
	bassert(!scope_is_local(scope) && "Injection destination scope must be global!");
	mtx_lock(&scope->injection_lock);
	for (usize i = 0; i < arrlenu(scope->injected); ++i) {
		if (other == scope->injected[i]) {
			mtx_unlock(&scope->injection_lock);
			return;
		}
	}
	// blog(">> INJECT to [%s]: " STR_FMT, scope_kind_name(scope), STR_ARG(other->_debug_name));
	arrput(scope->injected, other);
	mtx_unlock(&scope->injection_lock);
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
	case SCOPE_NAMED:
		return "Named";
	case SCOPE_FILE:
		return "File";
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

#ifdef BL_DEBUG
void scope_print(struct scope *scope) {
	blog("Scope: %s", scope_kind_name(scope));
	blog("Injected:");
	for (usize i = 0; i < arrlenu(scope->injected); ++i) {
		struct scope *injected = scope->injected[i];
		blog("  [%d] " STR_FMT, i, STR_ARG(injected->_debug_name));
	}

	blog("Symbols:");
	for (usize i = 0; i < tbl_len(scope->entries); ++i) {
		blog("  [%d] " STR_FMT, i, STR_ARG(scope->entries[i].key));
	}
}
#endif