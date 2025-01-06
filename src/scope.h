// =================================================================================================
// bl
//
// File:   scope.h
// Author: Martin Dorazil
// Date:   15/03/2018
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

#ifndef BL_SCOPE_H
#define BL_SCOPE_H

#include "arena.h"
#include "common.h"
#include "llvm_api.h"

struct location;
struct ast;
struct mir_instr;
struct mir_type;
struct mir_fn;
struct mir_var;

// Global context data used by all scopes in assembly.
struct scope_arenas {
	struct arena scopes;
	struct arena entries;
};

enum scope_entry_kind {
	SCOPE_ENTRY_INCOMPLETE,
	SCOPE_ENTRY_TYPE,
	SCOPE_ENTRY_VAR,
	SCOPE_ENTRY_FN,
	SCOPE_ENTRY_MEMBER,
	SCOPE_ENTRY_VARIANT,
	SCOPE_ENTRY_NAMED_SCOPE,
	SCOPE_ENTRY_UNNAMED, // Special kind used for unnamed entries.
	SCOPE_ENTRY_ARG,
};

union scope_entry_data {
	struct mir_type    *type;
	struct mir_fn      *fn;
	struct mir_var     *var;
	struct mir_member  *member;
	struct mir_variant *variant;
	struct mir_arg     *arg;
	struct scope       *scope;
};

struct scope_entry {
	struct id             *id;
	enum scope_entry_kind  kind;
	struct scope          *parent_scope;
	struct ast            *node;
	bool                   is_builtin;
	union scope_entry_data data;
	s32                    ref_count;
	bmagic_member
};

struct scope_tbl_entry {
	// Hash value of the scope entry as combination of layer and id.
	u64                 hash;
	str_t               key;
	struct scope_entry *value;
};

enum scope_kind {
	SCOPE_NONE = 0,
	SCOPE_GLOBAL,
	SCOPE_PRIVATE,
	SCOPE_FN,
	SCOPE_FN_BODY,
	SCOPE_LEXICAL,
	SCOPE_TYPE_STRUCT,
	SCOPE_TYPE_ENUM,
	SCOPE_MODULE,
	SCOPE_MODULE_PRIVATE,
};

#define SCOPE_DEFAULT_LAYER 0

struct scope {
	enum scope_kind  kind;
	str_t            name;     // optional
	str_t            filename; // optional
	struct scope    *parent;
	struct location *location;
	array(struct scope *) injected;
	LLVMMetadataRef llvm_meta;
	hash_table(struct scope_tbl_entry) entries;

	mtx_t lock;
	bmagic_member
};

void scope_arenas_init(struct scope_arenas *arenas, u32 owner_thread_index);
void scope_arenas_terminate(struct scope_arenas *arenas);

struct scope *scope_create(struct scope_arenas *arenas,
                           enum scope_kind      kind,
                           struct scope        *parent,
                           struct location     *loc);

struct scope_entry *scope_create_entry(struct scope_arenas  *arenas,
                                       enum scope_entry_kind kind,
                                       struct id            *id,
                                       struct ast           *node,
                                       bool                  is_builtin);

void scope_reserve(struct scope *scope, s32 num);
void scope_insert(struct scope *scope, hash_t layer, struct scope_entry *entry);
void scope_lock(struct scope *scope);
void scope_unlock(struct scope *scope);
void scope_inject(struct scope *dest, struct scope *src);

typedef struct {
	hash_t     layer;
	struct id *id;
	bool       in_tree;
	bool       local_only;
	bool      *out_of_function;
} scope_lookup_args_t;

// Returns number of entries written into out_buf.
s32 scope_lookup(struct scope *scope, scope_lookup_args_t *args, struct scope_entry **out_buf, const s32 oit_buf_size);

// Checks whether passed scope is of kind or is nested in scope of kind.
bool          scope_is_subtree_of_kind(const struct scope *scope, enum scope_kind kind);
bool          scope_is_subtree_of(const struct scope *scope, const struct scope *other);
struct scope *scope_find_closest_global(struct scope *scope);
const char   *scope_kind_name(const struct scope *scope);
// Resolve the full name of the scope (containing all namespaces separated by '.'.
void scope_get_full_name(str_buf_t *buf, struct scope *scope);

static inline bool scope_is_local(const struct scope *scope) {
	return scope->kind != SCOPE_GLOBAL && scope->kind != SCOPE_PRIVATE && scope->kind != SCOPE_MODULE && scope->kind != SCOPE_MODULE_PRIVATE;
}

static inline struct scope_entry *scope_entry_ref(struct scope_entry *entry) {
	bmagic_assert(entry);
	++entry->ref_count;
	return entry;
}

#endif
