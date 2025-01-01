// =================================================================================================
// bl
//
// File:   ast.h
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

#ifndef BL_AST_H
#define BL_AST_H

#include "arena.h"
#include "common.h"

#define AST_IS_BAD(node) ((node) && (node)->kind == AST_BAD)
#define AST_IS_OK(node) ((node) && (node)->kind != AST_BAD)

struct scope;
struct token;
struct location;
struct ast;

enum ast_kind {
#define GEN_AST_KINDS
#include "ast.def"
#undef GEN_AST_KINDS
};

enum ast_flags {
	FLAG_EXTERN = 1 << 0, // methods marked as extern
	// 1 << 1, free
	FLAG_COMPILER     = 1 << 2,  // compiler internal
	FLAG_PRIVATE      = 1 << 3,  // declared in private scope (might be nested in named scope!)
	FLAG_INLINE       = 1 << 4,  // inline function
	FLAG_NO_INLINE    = 1 << 5,  // no inline function
	FLAG_ENTRY        = 1 << 6,  // marking entry point function
	FLAG_BUILD_ENTRY  = 1 << 7,  // marking build entry point function
	FLAG_NO_INIT      = 1 << 8,  // no default initialization
	FLAG_INTRINSIC    = 1 << 9,  // intrinsics declaration
	FLAG_TEST_FN      = 1 << 10, // test function
	FLAG_EXPORT       = 1 << 11, // symbols marked for dll export
	FLAG_THREAD_LOCAL = 1 << 12, // symbols marked as thread local
	FLAG_FLAGS        = 1 << 13, // enum flags
	FLAG_COMPTIME     = 1 << 14, // compile-time execution
	FLAG_MAYBE_UNUSED = 1 << 15, // to markup unused declarations
	FLAG_OBSOLETE     = 1 << 16, // obsolete functions
};

// map symbols to binary operation kind
enum binop_kind {
	BINOP_INVALID = 0,
	BINOP_ASSIGN,
	BINOP_ADD_ASSIGN,
	BINOP_SUB_ASSIGN,
	BINOP_MUL_ASSIGN,
	BINOP_DIV_ASSIGN,
	BINOP_MOD_ASSIGN,
	BINOP_AND_ASSIGN,
	BINOP_OR_ASSIGN,
	BINOP_XOR_ASSIGN,
	BINOP_ADD,
	BINOP_SUB,
	BINOP_MUL,
	BINOP_DIV,
	BINOP_MOD,
	BINOP_EQ,
	BINOP_NEQ,
	BINOP_GREATER,
	BINOP_LESS,
	BINOP_GREATER_EQ,
	BINOP_LESS_EQ,
	BINOP_LOGIC_AND,
	BINOP_LOGIC_OR,
	BINOP_AND,
	BINOP_OR,
	BINOP_XOR,
	BINOP_SHR,
	BINOP_SHL,
};

enum unop_kind {
	UNOP_INVALID = 0,
	UNOP_NEG,
	UNOP_POS,
	UNOP_NOT,
	UNOP_BIT_NOT,
};

struct ast_docs {
	str_t text;
};

struct ast_load {
	str_t filepath;
};

struct ast_import {
	const char *filepath;
};

struct ast_private {
	void *_;
};

struct ast_public {
	void *_;
};

struct ast_scope {
	struct ast *ident;
};

struct ast_link {
	const char *lib;
};

struct ast_ident {
	struct id id;

	// Optional other identificator (group);
	struct ast *next;
};

struct ast_ref {
	struct ast *ident;
	struct ast *next;
};

struct ast_ublock {
	array(struct ast *) nodes;
	struct unit *unit;
};

struct ast_block {
	ast_nodes_t *nodes;
	bool         has_return;
};

struct ast_stmt_using {
	struct ast *scope_expr;
};

struct ast_stmt_return {
	// Optional return values.
	ast_nodes_t *exprs;
	struct ast  *fn_decl;
	struct ast  *owner_block;
};

struct ast_stmt_defer {
	struct ast *expr;
};

struct ast_stmt_if {
	struct ast *test;
	struct ast *true_stmt;
	struct ast *false_stmt;
	bool        is_static;
	bool        is_expression; // ternary
};

struct ast_stmt_switch {
	struct ast  *expr;
	ast_nodes_t *cases;
};

struct ast_stmt_case {
	ast_nodes_t *exprs;
	struct ast  *block;
	bool         is_default;
};

struct ast_stmt_loop {
	struct ast *init;
	struct ast *condition;
	struct ast *increment;
	struct ast *block;
};

struct ast_decl {
	struct ast    *name;
	struct ast    *type;
	struct ast    *tag; // Optional.
	enum ast_flags flags;
};

struct ast_decl_entity {
	struct ast_decl base;
	struct ast     *value;
	struct ast     *explicit_linkage_name; // Optional.
	bool            is_global;
	bool            mut;
};

struct ast_decl_member {
	struct ast_decl base;
};

struct ast_decl_arg {
	struct ast_decl base;
	struct ast     *value;
	u32             index;
};

struct ast_decl_variant {
	struct ast_decl base;
	struct ast     *value;
};

struct ast_type_arr {
	struct ast *elem_type;
	struct ast *len;

	// Array element count might be inferred from count of elements of compound initializer.
	// This way we allow [_]s32.{ 1, 2, 3 } syntax to automatically "replace" underscore with
	// number of elements.
	//
	// Might be true only when type is used in compound initializer, otherwise error should be
	// reported to the user.
	bool is_len_inferred_from_compound;
};

struct ast_type_poly {
	struct ast *ident;
};

struct ast_type_slice {
	struct ast *elem_type;
};

struct ast_type_dynarr {
	struct ast *elem_type;
};

enum ast_type_fn_flavor {
	AST_TYPE_FN_FLAVOR_NONE = 0,
	// Function type contains polymorph types.
	AST_TYPE_FN_FLAVOR_POLYMORPH = 1 << 1,
	// Function type contains comptime arguments.
	AST_TYPE_FN_FLAVOR_MIXED = 1 << 2,
};

struct ast_type_fn {
	struct ast             *ret_type;
	ast_nodes_t            *args;
	enum ast_type_fn_flavor flavor; // @Note: see ast_type_fn_flavor
};

struct ast_type_fn_group {
	ast_nodes_t *variants;
};

struct ast_type_struct {
	struct scope *scope;
	// Can contain also non-members i.e. using.
	ast_nodes_t *members;
	struct ast  *base_type;
	bool         is_union;
	bool         is_multiple_return_type;
};

struct ast_type_enum {
	struct scope *scope;
	struct ast   *type;
	ast_nodes_t  *variants;
	bool          is_flags;
};

struct ast_type_ptr {
	struct ast *type;
};

struct ast_type_vargs {
	struct ast *type;
};

struct ast_expr_type {
	struct ast *type;
};

struct ast_expr_compound {
	struct ast  *type; // Optional
	ast_nodes_t *values;
	// Allow type infer from function return type.
	bool is_multiple_return_value;
};

struct ast_expr_lit_fn {
	struct ast *type;
	struct ast *block;
	struct ast *obsolete_warning_message; // Optional
	struct ast *enable_if;
};

struct ast_expr_lit_fn_group {
	ast_nodes_t *variants;
};

struct ast_expr_lit_int {
	u64  val;
};

struct ast_expr_lit_float {
	f32  val;
};

struct ast_expr_lit_double {
	f64  val;
};

struct ast_expr_lit_char {
	u8 val;
};

struct ast_expr_lit_string {
	str_t val;
};

struct ast_expr_lit_bool {
	bool val;
};

struct ast_expr_cast {
	struct ast *type;
	struct ast *next;
	bool        auto_cast;
};

struct ast_expr_binop {
	struct ast     *lhs;
	struct ast     *rhs;
	enum binop_kind kind;
};

struct ast_expr_call {
	struct ast  *ref;
	ast_nodes_t *args;
};

struct ast_expr_elem {
	struct ast *next;
	struct ast *index;
};

struct ast_expr_unary {
	enum unop_kind kind;
	struct ast    *next;
};

struct ast_expr_addrof {
	struct ast *next;
};

struct ast_expr_deref {
	struct ast *next;
};

struct ast_tag {
	struct ast *expr;
};

struct ast_call_loc {
	void *_;
};

// struct ast base type
struct ast {
	enum ast_kind    kind;
	struct location *location;
	struct scope    *owner_scope;
	str_t            docs; // Optional documentation string.

	union {
#define GEN_AST_DATA
#include "ast.def"
#undef GEN_AST_DATA
	} data;

#ifdef BL_DEBUG
	u64 _serial;
#endif
};

void        ast_arena_init(struct arena *arena, u32 owner_thread_index);
void        ast_arena_terminate(struct arena *arena);
struct ast *ast_create_node(struct arena *arena,
                            enum ast_kind c,
                            struct token *tok,
                            struct scope *parent_scope);
const char *ast_binop_to_str(enum binop_kind op);
const char *ast_unop_to_str(enum unop_kind op);
const char *ast_get_name(const struct ast *n);

static inline bool ast_binop_is_logic(enum binop_kind op) {
	return op >= BINOP_EQ && op <= BINOP_LOGIC_OR;
}

#endif
