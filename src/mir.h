// =================================================================================================
// bl
//
// File:   mir.h
// Author: Martin Dorazil
// Date:   3/15/18
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

#ifndef BL_MIR_H
#define BL_MIR_H

#include "arena.h"
#include "ast.h"
#include "common.h"
#include "scope.h"
#include "vm.h"
#include <dyncall.h>
#include <dyncall_callback.h>
#include <dynload.h>

// Slice member indices, these match also for dynamic arrays and strings.
#define MIR_SLICE_LEN_INDEX 0
#define MIR_SLICE_PTR_INDEX 1

#define MIR_NO_REF_COUNTING (-1)

#ifdef BL_DEBUG
vm_stack_ptr_t _mir_cev_read(struct mir_const_expr_value *value);
#else
	#define _mir_cev_read(expr) (expr)->data
#endif

// Helper macro for reading Const Expression Values of fundamental types.
#define MIR_CEV_READ_AS(T, src) (*((T *)_mir_cev_read(src)))
#define MIR_CEV_WRITE_AS(T, dest, src) (*((T *)(dest)->data) = (src))

struct assembly;
struct builder;
struct unit;
struct location;

struct mir_type;
struct mir_member;
struct mir_variant;
struct mir_arg;
struct mir_var;
struct mir_fn;
struct mir_fn_group;
struct mir_fn_generated_recipe;
struct mir_const_expr_value;

#define GEN_INSTR(kind, name) struct name;
#include "mir.def"
#undef GEN_INSTR

struct mir_arenas {
	struct arena instr;
	struct arena type;
	struct arena var;
	struct arena fn;
	struct arena member;
	struct arena variant;
	struct arena arg;
	struct arena fn_group;
	struct arena fn_generated;
};

struct mir_type_cache_entry {
	hash_t           hash;
	str_t            key;
	struct mir_type *type;
};

struct mir_rtti_incomplete {
	struct mir_var  *var;
	struct mir_type *type;
};

typedef sarr_t(struct mir_instr *, 32) instrs_t;
typedef sarr_t(struct mir_rtti_incomplete, 64) mir_rttis_t;

struct mir_analyze {
	// Instructions waiting for analyze.
	array(struct mir_instr *) stack[2];
	s32 si; // Current stack index
	// Hash table of arrays. Hash is id of symbol and array contains queue of waiting
	// instructions.
	hash_table(struct {
		hash_t   key;
		instrs_t value;
	}) waiting;

	// Structure members can sometimes point to self, in such case we end up with
	// endless looping RTTI generation, to solve this problem we create dummy RTTI
	// variable for all pointer types and store them in this array. When structure RTTI
	// is complete we can fill missing pointer RTTIs in second generation pass.
	mir_rttis_t incomplete_rtti;

	// Incomplete type check stack.
	mir_types_t          complete_check_type_stack;
	struct scope_entry **usage_check_arr;
	struct scope_entry  *unnamed_entry;

	// Table of instruction being skipped in analyze pass, this should be empty at the end
	// of analyze!
	hash_table(struct {
		struct mir_instr *key;
		u8                value; // this is not used
	}) skipped_instructions;
};

struct mir {
	struct mir_arenas arenas;
	array(struct mir_instr *) global_instrs; // All global instructions.
	spl_t global_instrs_lock;

	struct {
		hash_t          key;
		struct mir_var *value;
	}    *rtti_table; // Map type ids to RTTI variables.
	spl_t rtti_table_lock;

	array(struct mir_instr *) exported_instrs;
	spl_t exported_instrs_lock;

	my_hash_table(struct mir_type_cache_entry) type_cache;
	spl_t type_cache_lock;

	struct mir_analyze analyze;
	struct mir_sync   *sync;
};

struct mir_switch_case {
	struct mir_instr       *on_value;
	struct mir_instr_block *block;
};

typedef sarr_t(struct mir_switch_case, 32) mir_switch_cases_t;

enum mir_type_kind {
	MIR_TYPE_INVALID     = 0,
	MIR_TYPE_TYPE        = 1,
	MIR_TYPE_VOID        = 2,
	MIR_TYPE_INT         = 3,
	MIR_TYPE_REAL        = 4,
	MIR_TYPE_FN          = 5,
	MIR_TYPE_PTR         = 6,
	MIR_TYPE_BOOL        = 7,
	MIR_TYPE_ARRAY       = 8,
	MIR_TYPE_STRUCT      = 9,
	MIR_TYPE_ENUM        = 10,
	MIR_TYPE_NULL        = 11,
	MIR_TYPE_STRING      = 12,
	MIR_TYPE_VARGS       = 13,
	MIR_TYPE_SLICE       = 14,
	MIR_TYPE_DYNARR      = 15,
	MIR_TYPE_FN_GROUP    = 16,
	MIR_TYPE_NAMED_SCOPE = 17,
	MIR_TYPE_POLY        = 18,
	MIR_TYPE_PLACEHOLDER = 19,
};

// External function arguments passing composite types by value needs special handling in IR.
enum llvm_extern_arg_struct_generation_mode {
	LLVM_EASGM_NONE,  // No special handling
	LLVM_EASGM_8,     // Promote composite as i8
	LLVM_EASGM_16,    // Promote composite as i16
	LLVM_EASGM_32,    // Promote composite as i32
	LLVM_EASGM_64,    // Promote composite as i64
	LLVM_EASGM_64_8,  // Promote composite as i64, i8
	LLVM_EASGM_64_16, // Promote composite as i64, i16
	LLVM_EASGM_64_32, // Promote composite as i64, i32
	LLVM_EASGM_64_64, // Promote composite as i64, i64
	LLVM_EASGM_BYVAL, // Promote composite as byval
};

enum mir_instr_kind {
	MIR_INSTR_INVALID = 0,
#define GEN_INSTR_KINDS
#include "mir.def"
#undef GEN_INSTR_KINDS
};

enum mir_cast_op {
	MIR_CAST_INVALID,
	MIR_CAST_NONE,
	MIR_CAST_BITCAST,
	MIR_CAST_SEXT,
	MIR_CAST_ZEXT,
	MIR_CAST_TRUNC,
	MIR_CAST_FPTRUNC,
	MIR_CAST_FPEXT,
	MIR_CAST_FPTOSI,
	MIR_CAST_FPTOUI,
	MIR_CAST_SITOFP,
	MIR_CAST_UITOFP,
	MIR_CAST_PTRTOINT,
	MIR_CAST_INTTOPTR,
	MIR_CAST_PTRTOBOOL,
};

enum builtin_id_kind {
	BUILTIN_ID_NONE = -1,
#define GEN_BUILTIN_NAMES
#include "builtin.def"
#undef GEN_BUILTIN_NAMES
	_BUILTIN_ID_COUNT,
};

extern struct id builtin_ids[_BUILTIN_ID_COUNT];

struct dyncall_cb_context {
	struct virtual_machine *vm;
	struct mir_fn          *fn;
};

struct mir_fn_generated_recipe {
	// Function literal (used for function replacement generation).
	struct ast *ast_lit_fn;
	// Scope layer solves symbol collisions in reused scopes.
	hash_t scope_layer;
	// Cache of already generated functions (replacement hash -> struct mir_fn*).
	hash_table(struct {
		hash_t         key;
		struct mir_fn *value;
	}) entries;

	bmagic_member
};

enum mir_fn_generated_flavor_flags {
	MIR_FN_GENERATED_NONE  = 0,
	MIR_FN_GENERATED_POLY  = 1 << 1,
	MIR_FN_GENERATED_MIXED = 1 << 2,
};

// FN
// @Performance: Make smaller version of the function for function recipes.
struct mir_fn {
	// Must be first!!!
	struct mir_instr   *prototype;
	struct id          *id;
	struct ast         *decl_node;
	struct scope_entry *scope_entry;

	// Optional, set only in case this function instance is only "recipe" and it is later used to
	// generate actual implementation based on some compile time known requirements. I.e.
	// polymorphic type replacement or comptime value replacement.
	struct mir_fn_generated_recipe *generation_recipe;

	// Describe compile-time generated function, set for polymorph, mixed and comptime-called
	// function.
	enum mir_fn_generated_flavor_flags generated_flavor;

	// This structure is initialized only in case this function is generated from polymorphic
	// function recipe, it's not polymorph anymore (its type is also not polymorph).
	struct {
		// Optional, this is set to first call location used for generation of this function from
		// polymorph recipe.
		struct ast *first_call_node;

		// Optional, simple stringification of the original polymorph argument types used to
		// generate this function, this may be useful to produce informative error messages. In case
		// the function is comptime or has mixed arguments without any polymorph replacements this
		// string may be NULL.
		str_t debug_replacement_types;
	} generated;

	// function body scope if there is one (optional)
	struct scope    *body_scope;
	struct mir_type *type;
	array(struct mir_var *) variables;
	// Linkage name of the function, this name is used during linking to identify function,
	// actual implementation can be external, internal or intrinsic embedded in compiler,
	// depending on function flags.
	str_t linkage_name;
	// Full name contains full function name with parent scopes.
	str_t                full_name;
	LLVMValueRef         llvm_value;
	bool                 is_fully_analyzed;
	bool                 is_global;
	bool                 is_disabled; // Set based on optional enable_if expression in function prototype.
	s32                  ref_count;
	enum ast_flags       flags;
	enum builtin_id_kind builtin_id;
	// pointer to the first block inside function body
	struct mir_instr_block *first_block;
	struct mir_instr_block *last_block;
	struct mir_instr_block *exit_block;
	// Temporary variable used for return value.
	struct mir_instr *ret_tmp;
	// Return instruction of function.
	struct mir_instr_ret *terminal_instr;

	// @Performance: This is needed only for external functions!
	struct {
		DCpointer                 extern_entry;
		DCCallback               *extern_callback_handle;
		struct dyncall_cb_context context;
	} dyncall;              // dyncall external context
	str_t obsolete_message; // Optional, check len!

	bmagic_member
};

struct mir_fn_group {
	struct ast *decl_node;
	mir_fns_t  *variants;
	bmagic_member
};

// MEMBER
struct mir_member {
	struct mir_type    *type;
	struct id          *id;
	struct ast         *decl_node;
	struct scope_entry *entry;
	s64                 index;
	u64                 tag;
	s32                 offset_bytes;
	bool                is_base; // inherited struct base
	bool                is_parent_union;
	bmagic_member
};

// FUNCTION ARGUMENT
struct mir_arg {
	struct mir_type    *type;
	struct id          *id;
	struct ast         *decl_node;
	struct scope_entry *entry;
	u32                 index;
	bool                is_inside_recipe; // True in case the argument is part of function recipe.
	bool                is_inside_declaration;
	s32                 ref_count;
	enum ast_flags      flags;
	// This is index of this argument in LLVM IR not in MIR, it can be different based on
	// compiler configuration.
	u32 llvm_index;
	// Optional default value.
	struct mir_instr *default_value;
	// Optional, set only for generated functions.
	struct mir_instr_call                      *generation_call;
	enum llvm_extern_arg_struct_generation_mode llvm_easgm;
	u64                                         backend_value;
	bmagic_member
};

// TYPE
struct mir_type_int {
	s32  bitcount;
	bool is_signed;
};

struct mir_type_real {
	s32 bitcount;
};

struct mir_type_fn {
	struct mir_type *ret_type;
	mir_args_t      *args;
	hash_t           argument_hash;
	enum ast_flags   flags;

	enum builtin_id_kind builtin_id;

	// @Performance: Rewrite to flags.
	bool is_vargs;
	// Polymorph function type (not all arguments have known type -> cannot generate type info).
	bool is_polymorph;
	bool has_default_args;
	bool has_byval;
	bool has_sret;
};

struct mir_type_poly {
	bool is_master;
};

struct mir_type_fn_group {
	mir_types_t *variants;
};

struct mir_type_named_scope {
	void *_;
};

struct mir_type_placeholder {
	void *_;
};

struct mir_type_ptr {
	struct mir_type *expr;
};

struct mir_type_struct {
	struct scope  *scope; // struct body scope
	hash_t         scope_layer;
	mir_members_t *members;
	// This is optional base type, only structures with #base hash directive has this
	// information.
	struct mir_type *base_type;

	// @Performance: Rewrite to flags.
	bool is_packed;
	// C-style union is represented as regular structure with special memory layout. Every
	// member is stored at same memory offset.
	bool is_union;
	// Set true only for incomplete forward declarations of the struct.
	bool is_incomplete_fwd_struct;
	// Set true for struct type used as multiple return temporary.
	bool is_multiple_return_type;
	// Set true for string literals (represented as slice of u8 values).
	bool is_string_literal;
};

// Enum variants must be baked into enum type.
struct mir_type_enum {
	struct scope *scope;
	// @Incomplete: missing scope_layer!
	struct mir_type *base_type;
	mir_variants_t  *variants;
	bool             is_flags;
};

struct mir_type_null {
	struct mir_type *base_type;
};

struct mir_type_array {
	struct mir_type *elem_type;
	s64              len;
};

struct mir_type {
	struct id         *user_id;
	struct id          id;
	LLVMTypeRef        llvm_type;
	LLVMMetadataRef    llvm_meta;
	usize              size_bits;
	usize              store_size_bytes;
	enum mir_type_kind kind;
	s8                 alignment;
	bool               checked_and_complete;

	// In case this is true, the type does not hold any unique state (e.g. s32, pointer to s32,
	// etc.) and thus it can be cached instead of created each time when needed.
	bool can_use_cache;

	union {
		struct mir_type_int         integer;
		struct mir_type_fn          fn;
		struct mir_type_fn_group    fn_group;
		struct mir_type_ptr         ptr;
		struct mir_type_real        real;
		struct mir_type_array       array;
		struct mir_type_struct      strct;
		struct mir_type_enum        enm;
		struct mir_type_null        null;
		struct mir_type_named_scope named_scope;
		struct mir_type_poly        poly;
		struct mir_type_placeholder placeholder;
	} data;

	bmagic_member
};

// VARIANT
struct mir_variant {
	struct id          *id;
	struct scope_entry *entry;
	struct mir_type    *value_type;
	u64                 value;
};

enum mir_var_flags {
	MIR_VAR_MUTABLE        = 1 << 0,
	MIR_VAR_GLOBAL         = 1 << 1,
	MIR_VAR_IMPLICIT       = 1 << 2,
	MIR_VAR_STRUCT_TYPEDEF = 1 << 3,
	MIR_VAR_ANALYZED       = 1 << 4,

	// We need this to allow the return temporary variable to be mutable even if it contains types;
	// the type can be returned only from compile-time functions. Value is set 'true' only when the
	// temporary return variable is created and used only for checking in 'analyze_var'.
	MIR_VAR_RET_TMP = 1 << 5,
	// Mark the variable as function argument temporary.
	MIR_VAR_ARG_TMP = 1 << 6,
	// Keep this, we sometimes have i.e. type defs in scope of the function.
	MIR_VAR_EMIT_LLVM = 1 << 7,
};

// VAR
struct mir_var {
	struct mir_const_expr_value value; // contains also allocated type
	struct id                  *id;
	struct ast                 *decl_node;
	struct scope               *decl_scope;
	struct scope_entry         *entry;
	struct mir_instr           *initializer_block;
	union {
		LLVMValueRef llvm_value;
		u64          backend_value;
	};
	str_t                linkage_name;
	enum builtin_id_kind builtin_id;
	enum ast_flags       flags;  // User flags.
	enum mir_var_flags   iflags; // Internal flags.
	union {
		vm_stack_ptr_t          global;
		vm_relative_stack_ptr_t local;
	} vm_ptr;
	s32 arg_index;
	s32 ref_count;
};

enum mir_instr_state {
	MIR_IS_PENDING  = 0,
	MIR_IS_ANALYZED = 1,
	MIR_IS_COMPLETE = 3,
	MIR_IS_FAILED   = 4,
	MIR_IS_ERASED   = 5,
};

struct mir_instr {
	struct mir_const_expr_value value;
	enum mir_instr_kind         kind;
	enum mir_instr_state        state;
	u64                         id;
	struct ast                 *node;
	struct mir_instr_block     *owner_block;

	union {
		LLVMValueRef llvm_value;
		u64          backend_value;
	};

	struct mir_instr *prev;
	struct mir_instr *next;

#if defined(BL_DEBUG) || defined(BL_ASSERT_ENABLE)
	enum mir_instr_kind _orig_kind;
#endif
	s32  ref_count;
	bool is_implicit;
	bmagic_member
};

struct mir_instr_block {
	struct mir_instr  base;
	str_t             name;
	struct mir_instr *entry_instr;
	struct mir_instr *last_instr;
	struct mir_instr *terminal;
	// Optional; when not set block is implicit global block.
	struct mir_fn *owner_fn;
	// 2024-08-07 Used only for code explicitly writen by user as unreachable. This does not apply to
	// blocks becoming unreferenced by optimizations is analyze pass.
	bool is_unreachable;
};

struct mir_instr_decl_var {
	struct mir_instr  base;
	struct mir_var   *var;
	struct mir_instr *type;
	struct mir_instr *init;
};

struct mir_instr_decl_member {
	struct mir_instr   base;
	struct mir_member *member;
	struct mir_instr  *type;

	struct mir_instr *tag; // Optional.
};

struct mir_instr_decl_variant {
	struct mir_instr    base;
	struct mir_variant *variant;
	struct mir_variant *prev_variant; // Optional.
	struct mir_instr   *value;        // Optional.
	struct mir_instr   *base_type;    // Optional.
	bool                is_flags;
};

struct mir_instr_decl_arg {
	struct mir_instr  base;
	struct mir_arg   *arg;
	struct mir_instr *type;
};

struct mir_instr_elem_ptr {
	struct mir_instr  base;
	struct mir_instr *arr_ptr;
	struct mir_instr *index;
};

struct mir_instr_member_ptr {
	struct mir_instr     base;
	struct ast          *member_ident;
	struct mir_instr    *target_ptr;
	struct scope_entry  *scope_entry;
	enum builtin_id_kind builtin_id;
};

struct mir_instr_cast {
	struct mir_instr  base;
	enum mir_cast_op  op;
	struct mir_instr *type;
	struct mir_instr *expr;
	bool              auto_cast;
};

struct mir_instr_sizeof {
	struct mir_instr  base;
	mir_instrs_t     *args; // Used only as temporary for analyze.
	struct mir_instr *expr;

	// Used mainly to skip input expression analyze pass in case the type was incomplete.
	struct mir_type *resolved_type;
};

struct mir_instr_alignof {
	struct mir_instr  base;
	mir_instrs_t     *args; // Used only as temporary for analyze.
	struct mir_instr *expr;
};

struct mir_instr_typeof {
	struct mir_instr  base;
	mir_instrs_t     *args; // Used only as temporary for analyze.
	struct mir_instr *expr;
};

enum mir_user_msg_kind {
	MIR_USER_MSG_WARNING,
	MIR_USER_MSG_ERROR,
};

// Contains user defined compile time message.
struct mir_instr_msg {
	struct mir_instr       base;
	mir_instrs_t          *args; // Used only as temporary for analyze.
	enum mir_user_msg_kind message_kind;
	struct mir_instr      *expr;
};

struct mir_instr_arg {
	struct mir_instr base;
	unsigned         i;
};

struct mir_instr_const {
	struct mir_instr base;
	// Constant marked as volatile can change it's type as needed by expression.
	bool volatile_type;
};

struct mir_instr_load {
	struct mir_instr  base;
	struct mir_instr *src;
	// This flag is set when load is user-level dereference.
	bool is_deref;
};

struct mir_instr_store {
	struct mir_instr  base;
	struct mir_instr *src;
	struct mir_instr *dest;
};

struct mir_instr_addrof {
	struct mir_instr  base;
	struct mir_instr *src;
};

struct mir_instr_ret {
	struct mir_instr  base;
	struct mir_instr *value;
	bool              expected_comptime;
};

struct mir_instr_set_initializer {
	struct mir_instr  base;
	struct mir_instr *dest;
	struct mir_instr *src;
};

struct mir_instr_binop {
	struct mir_instr  base;
	enum binop_kind   op;
	struct mir_instr *lhs;
	struct mir_instr *rhs;
	bool              volatile_type;
	bool              is_condition;
};

struct mir_instr_unop {
	struct mir_instr  base;
	enum unop_kind    op;
	struct mir_instr *expr;
	bool              volatile_type;
	bool              is_condition;
};

struct mir_instr_fn_proto {
	struct mir_instr  base;
	struct mir_instr *type;
	struct mir_instr *user_type;
	struct mir_instr *enable_if;
	bool              pushed_for_analyze;
};

struct mir_instr_fn_group {
	struct mir_instr base;
	mir_instrs_t    *variants;
};

struct mir_instr_type_fn {
	struct mir_instr     base;
	struct mir_instr    *ret_type;
	mir_instrs_t        *args;
	enum builtin_id_kind builtin_id;
	bool                 is_polymorph;
	bool                 is_inside_declaration;
};

struct mir_instr_type_fn_group {
	struct mir_instr base;
	struct id       *id;
	mir_instrs_t    *variants;
};

struct mir_instr_type_struct {
	struct mir_instr base;
	// fwd_decl is optional pointer to forward declaration of this structure type.
	struct mir_instr *fwd_decl;
	struct id        *user_id;
	struct scope     *scope;
	hash_t            scope_layer;
	mir_instrs_t     *members;
	bool              is_packed;
	bool              is_union;
	// Set true for struct type used as multiple return temporary.
	bool is_multiple_return_type;
};

struct mir_instr_type_enum {
	struct mir_instr  base;
	struct id        *user_id;
	struct scope     *scope;
	mir_instrs_t     *variants;
	struct mir_instr *base_type;
	bool              is_flags;
};

struct mir_instr_type_ptr {
	struct mir_instr  base;
	struct mir_instr *type;
};

struct mir_instr_type_array {
	struct mir_instr  base;
	struct mir_instr *elem_type;
	struct mir_instr *len;
	struct id        *id;
};

struct mir_instr_type_slice {
	struct mir_instr  base;
	struct mir_instr *elem_type;
};

struct mir_instr_type_dyn_arr {
	struct mir_instr  base;
	struct mir_instr *elem_type;
};

struct mir_instr_type_vargs {
	struct mir_instr  base;
	struct mir_instr *elem_type;
};

struct mir_instr_type_poly {
	struct mir_instr base;
	struct id       *T_id;
};

struct context;

typedef struct result (*mir_call_analyze_stage_fn_t)(struct context        *ctx,
                                                     struct mir_instr_call *call);

// @Note: Call instruction with set base.value.is_comptime will be automatically executed during
// analyze process.
struct mir_instr_call {
	struct mir_instr base;
	// Generally any callable instruction.
	struct mir_instr                  *callee;
	const mir_call_analyze_stage_fn_t *analyze_pipeline;

	// Pointer to called function resolved after overload resolution.
	struct mir_fn *called_function;
	mir_instrs_t  *args; // Optional

	// Optional temporary variable for unroll multi-return struct type.
	struct mir_instr *tmp_var;

	// True if the call is inside the function type recipe, we should not call it while evaluation
	// is done + we have to replace the result type by placeholder.
	bool is_inside_recipe;

	// clang-format off
	bcalled_once_member(prescan_args)
	bcalled_once_member(resolve_overload)
	bcalled_once_member(generate)
	bcalled_once_member(finalize)
	// clang-format on
};

struct mir_instr_decl_ref {
	struct mir_instr    base;
	struct unit        *parent_unit;
	struct id          *rid;
	struct scope       *scope;
	hash_t              scope_layer;
	struct scope_entry *scope_entry;

	// Set only for decl_refs inside struct member type resolver.
	bool accept_incomplete_type;
	// Set in case the named scope was specified explicitly.
	bool is_explicit;
};

struct mir_instr_decl_direct_ref {
	struct mir_instr  base;
	struct mir_instr *ref;
};

struct mir_instr_unreachable {
	struct mir_instr base;
	struct mir_fn   *abort_fn;
};

struct mir_instr_debugbreak {
	struct mir_instr base;
	struct mir_fn   *break_fn;
};

struct mir_instr_cond_br {
	struct mir_instr        base;
	struct mir_instr       *cond;
	struct mir_instr_block *then_block;
	struct mir_instr_block *else_block;

	// This value is used only during execution in Virtual Machine, when conditional break is
	// generated to be used as pre-instruction to PHI, we must keep condition value on stack (if
	// it's not compile time known) in order to be used as resolution of PHI expression.
	bool keep_stack_value;
	// Conditional break generated from static if.
	bool is_static;
};

struct mir_instr_br {
	struct mir_instr        base;
	struct mir_instr_block *then_block;
};

struct mir_instr_compound {
	struct mir_instr  base;
	struct mir_instr *type;

	// If there are no values explicitly specified, we assume that the compound is zero initialized,
	// in some cases we can do some optimizations based on that.
	mir_instrs_t *values;

	// Store destination indices of all values provided based on the compound type. Initialized in
	// analyze pass when the type is known.
	// In case the mapping is not specified (NULL) we process values one by one as they are.
	ints_t *value_member_mapping;

	struct mir_var *tmp_var;
	bool            is_naked;
	// Set when compound is used as multiple return value.
	bool is_multiple_return_value;
};

struct mir_instr_vargs {
	struct mir_instr base;
	struct mir_var  *arr_tmp;
	struct mir_var  *vargs_tmp;
	struct mir_type *type;
	mir_instrs_t    *values;
};

struct mir_instr_type_info {
	struct mir_instr  base;
	mir_instrs_t     *args; // Temporary
	struct mir_instr *expr;
	struct mir_type  *rtti_type;
};

struct mir_instr_test_case {
	struct mir_instr base;
};

struct mir_instr_call_loc {
	struct mir_instr base;
	struct location *call_location; // Optional call location
	struct mir_var  *meta_var;      // Optional meta var.
	str_t            function_name;
	hash_t           hash;
};

struct mir_instr_unroll {
	struct mir_instr  base;
	struct mir_instr *src;
	// Previous destination is optional reference to the previous variable in case we initialize
	// multiple variables at once by function call. i.e.: a, b, c := foo(); when 'foo' returns one
	// single value. Unroll instruction is removed is such case and variable using current unroll is
	// directly set to 'prev' value: c = b = a = foo();
	struct mir_instr *prev;
	s32               index;
	bool              remove;
};

struct mir_instr_using {
	struct mir_instr  base;
	struct scope     *scope;
	struct mir_instr *scope_expr;
};

// Used to distinguish compound initialization values with explicit name of member which should be
// initialized.
struct mir_instr_designator {
	struct mir_instr  base;
	struct ast       *ident;
	struct mir_instr *value;
};

struct mir_instr_phi {
	struct mir_instr base;

	int                     num;
	struct mir_instr       *incoming_values[2];
	struct mir_instr_block *incoming_blocks[2];

	// 2024-08-10 Phi instructions are currently constructed in the way that the first income block is
	// terminated by conditional break evertime, and we store it here.
	struct mir_instr *origin_br;
};

struct mir_instr_to_any {
	struct mir_instr  base;
	struct mir_instr *expr;
	struct mir_type  *rtti_type;
	// Temporary variable on the call-side used to store Any structure data.
	struct mir_var *tmp;

	// In case we convert Types to any value, this field contains pointer to the passed type,
	// we need to generated RTTI for this type and use it as .data.
	struct mir_type *rtti_data; // optional
	// Expression might be naked so it's duplicated into temporary variable in some cases.
	// (literals).
	struct mir_var *expr_tmp; // optional
};

struct mir_instr_switch {
	struct mir_instr        base;
	struct mir_instr       *value;
	struct mir_instr_block *default_block;
	mir_switch_cases_t     *cases;
	bool                    has_user_defined_default;
};

// public
static inline bool mir_is_pointer_type(const struct mir_type *type) {
	bassert(type);
	return type->kind == MIR_TYPE_PTR;
}

static inline bool mir_is_placeholder(const struct mir_instr *instr) {
	bassert(instr);
	bassert(instr->value.type);
	return instr->value.type->kind == MIR_TYPE_PLACEHOLDER;
}

static inline struct mir_type *mir_deref_type(const struct mir_type *ptr) {
	bassert(mir_is_pointer_type(ptr) && "Attempt to dereference non-pointer type!");
	return ptr->data.ptr.expr;
}

static inline bool mir_is_composite_type(const struct mir_type *type) {
	switch (type->kind) {
	case MIR_TYPE_STRUCT:
	case MIR_TYPE_STRING:
	case MIR_TYPE_SLICE:
	case MIR_TYPE_VARGS:
	case MIR_TYPE_DYNARR:
		return true;

	default:
		break;
	}

	return false;
}

static inline struct mir_type *mir_get_struct_elem_type(const struct mir_type *type, usize i) {
	bassert(mir_is_composite_type(type) && "Expected structure type");
	mir_members_t *members = type->data.strct.members;
	bassert(sarrlenu(members) > i);
	return sarrpeek(members, i)->type;
}

static inline struct mir_arg *mir_get_fn_arg(const struct mir_type *type, usize i) {
	bassert(type->kind == MIR_TYPE_FN && "Expected function type");
	mir_args_t *args = type->data.fn.args;
	if (!args) return NULL;
	bassert(sarrlenu(args) > i);
	return sarrpeek(args, i);
}

static inline struct mir_type *mir_get_fn_arg_type(const struct mir_type *type, usize i) {
	struct mir_arg *arg = mir_get_fn_arg(type, i);
	bassert(arg);
	return arg->type;
}

// Determinate if the instruction has compile time known value.
static inline bool mir_is_comptime(const struct mir_instr *instr) {
	bassert(instr);
	return instr->value.is_comptime;
}

static inline bool mir_is_global_block(const struct mir_instr_block *instr) {
	bassert(instr);
	return instr->owner_fn == NULL;
}

// Determinate if the instruction is in the global block.
static inline bool mir_is_global(const struct mir_instr *instr) {
	// Instructions without owner block lives in global scope.
	if (!instr->owner_block) return true;
	return mir_is_global_block(instr->owner_block);
}

static inline bool mir_type_has_llvm_representation(const struct mir_type *type) {
	bassert(type);
	return type->kind != MIR_TYPE_TYPE && type->kind != MIR_TYPE_FN_GROUP &&
	       type->kind != MIR_TYPE_NAMED_SCOPE && type->kind != MIR_TYPE_POLY;
}

static inline bool mir_type_cmp(const struct mir_type *first, const struct mir_type *second) {
	bassert(first && second);
	return first->id.hash == second->id.hash;
}

static inline bool mir_is_zero_initialized(const struct mir_instr_compound *compound) {
	bassert(compound);
	return compound->values == NULL;
}

static inline struct mir_fn *mir_instr_owner_fn(struct mir_instr *instr) {
	bassert(instr);
	if (instr->kind == MIR_INSTR_BLOCK) {
		return ((struct mir_instr_block *)instr)->owner_fn;
	}
	if (!instr->owner_block) return NULL;
	return instr->owner_block->owner_fn;
}

void            mir_init(struct assembly *assembly);
void            mir_terminate(struct assembly *assembly);
struct mir_var *mir_get_rtti(struct assembly *assembly, hash_t type_hash);
bool            mir_is_in_comptime_fn(struct mir_instr *instr);
str_buf_t       mir_type2str(const struct mir_type *type, bool prefer_name);
const char     *mir_instr_name(const struct mir_instr *instr);
void            mir_unit_run(struct assembly *assembly, struct unit *unit);
void            mir_analyze_run(struct assembly *assembly);
struct mir_fn  *mir_get_callee(const struct mir_instr_call *call);
str_t           mir_get_fn_readable_name(struct mir_fn *fn);

#endif
