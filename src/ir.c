// =================================================================================================
// bl
//
// File:   ir.c
// Author: Martin Dorazil
// Date:   12/9/18
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

#include "assembly.h"
#include "bldebug.h"
#include "builder.h"
#include "common.h"
#include "stb_ds.h"
#include "table.h"
#include "llvm-c/Types.h"

#ifdef BL_DEBUG
#define NAMED_VARS true
#else
#define NAMED_VARS false
#endif

#define STORE_MAX_SIZE_BYTES 16
#define DI_LOCATION_SET(instr)                       \
	if (ctx->generate_debug_info && (instr)->node) { \
		emit_DI_instr_loc(ctx, (instr));             \
	}                                                \
	(void)0

#define DI_LOCATION_RESET()                                    \
	if (ctx->generate_debug_info) {                            \
		LLVMSetCurrentDebugLocation2(ctx->llvm_builder, NULL); \
	}                                                          \
	(void)0

struct rtti_incomplete {
	LLVMValueRef     llvm_var;
	struct mir_type *type;
};

enum ir_state {
	STATE_PASSED,
	STATE_POSTPONE,
};

typedef sarr_t(LLVMValueRef, 16) llvm_values_t;
typedef sarr_t(LLVMMetadataRef, 16) llvm_metas_t;

struct cache_entry {
	hash_t       hash;
	str_t        key; // Might be not used...
	LLVMValueRef value;
};

struct ir_context {
	struct assembly *assembly;

	// Used for the 1st pass.
	queue_t(struct mir_instr *) queue;
	// Used for the 2nd pass.
	queue_t(struct mir_instr *) incomplete_queue;

	llvm_context_ref_t llvm_cnt;
	LLVMModuleRef      llvm_module;
	LLVMTargetDataRef  llvm_td;
	LLVMBuilderRef     llvm_builder;
	LLVMDIBuilderRef   llvm_di_builder;

	// Constants
	LLVMValueRef llvm_const_i64_zero;
	LLVMValueRef llvm_const_i8_zero;

	hash_table(struct cache_entry) gstring_cache;
	hash_table(struct cache_entry) llvm_fn_cache;
	array(struct rtti_incomplete) incomplete_rtti;

	struct builtin_types *builtin_types;
	bool                  generate_debug_info;
	array(struct mir_type *) di_incomplete_types;

	// intrinsics
	LLVMValueRef intrinsic_memset;
	LLVMTypeRef  intrinsic_memset_type;
	LLVMValueRef intrinsic_memcpy;
	LLVMTypeRef  intrinsic_memcpy_type;

	// stats
	s64 emit_instruction_count;
};

// =================================================================================================
// Tests
// =================================================================================================
static struct mir_var *testing_fetch_meta(struct ir_context *ctx);
static LLVMValueRef    testing_emit_meta_case(struct ir_context *ctx, struct mir_fn *fn);

// =================================================================================================
// Intrinsic helpers
// =================================================================================================
static LLVMValueRef build_call_memcpy(struct ir_context *ctx, LLVMValueRef src, LLVMValueRef dest, const usize size_bytes);

// =================================================================================================
// RTTI
// =================================================================================================
static LLVMValueRef rtti_emit(struct ir_context *ctx, struct mir_type *type);
static void         rtti_satisfy_incomplete(struct ir_context *ctx, struct rtti_incomplete incomplete);
static LLVMValueRef _rtti_emit(struct ir_context *ctx, struct mir_type *type);
static LLVMValueRef rtti_emit_base(struct ir_context *ctx,
                                   struct mir_type   *type,
                                   enum mir_type_kind kind,
                                   usize              size,
                                   s8                 alignment);
static LLVMValueRef rtti_emit_integer(struct ir_context *ctx, struct mir_type *type);
static LLVMValueRef rtti_emit_real(struct ir_context *ctx, struct mir_type *type);
static LLVMValueRef rtti_emit_array(struct ir_context *ctx, struct mir_type *type);
static LLVMValueRef rtti_emit_empty(struct ir_context *ctx, struct mir_type *type, struct mir_type *rtti_type);
static LLVMValueRef rtti_emit_enum(struct ir_context *ctx, struct mir_type *type);
static LLVMValueRef rtti_emit_enum_variant(struct ir_context *ctx, struct mir_variant *variant);
static LLVMValueRef rtti_emit_enum_variants_array(struct ir_context *ctx, mir_variants_t *variants);
static LLVMValueRef rtti_emit_enum_variants_slice(struct ir_context *ctx, mir_variants_t *variants);
static LLVMValueRef rtti_emit_struct(struct ir_context *ctx, struct mir_type *type);
static LLVMValueRef rtti_emit_struct_member(struct ir_context *ctx, struct mir_member *member);
static LLVMValueRef rtti_emit_struct_members_array(struct ir_context *ctx, mir_members_t *members);
static LLVMValueRef rtti_emit_struct_members_slice(struct ir_context *ctx, mir_members_t *members);
static LLVMValueRef rtti_emit_fn(struct ir_context *ctx, struct mir_type *type);
static LLVMValueRef rtti_emit_fn_arg(struct ir_context *ctx, struct mir_arg *arg);
static LLVMValueRef rtti_emit_fn_args_array(struct ir_context *ctx, mir_args_t *args);
static LLVMValueRef rtti_emit_fn_args_slice(struct ir_context *ctx, mir_args_t *args);
static LLVMValueRef rtti_emit_fn_group(struct ir_context *ctx, struct mir_type *type);
static LLVMValueRef rtti_emit_fn_slice(struct ir_context *ctx, mir_types_t *fns);
static LLVMValueRef rtti_emit_fn_array(struct ir_context *ctx, mir_types_t *fns);

// =================================================================================================
// Debug info
// =================================================================================================
static void            emit_DI_fn(struct ir_context *ctx, struct mir_fn *fn);    // @CLEANUP rename
static void            emit_DI_var(struct ir_context *ctx, struct mir_var *var); // @CLEANUP rename
static LLVMMetadataRef DI_type_init(struct ir_context *ctx, struct mir_type *type);
static LLVMMetadataRef DI_complete_type(struct ir_context *ctx, struct mir_type *type);
static LLVMMetadataRef DI_scope_init(struct ir_context *ctx, struct scope *scope);
static LLVMMetadataRef DI_unit_init(struct ir_context *ctx, struct unit *unit);

// =================================================================================================
// Instruction emit
// =================================================================================================
static enum ir_state emit_instr(struct ir_context *ctx, struct mir_instr *instr);
static LLVMValueRef  emit_const_string(struct ir_context *ctx, const str_t s);
static enum ir_state emit_instr_binop(struct ir_context *ctx, struct mir_instr_binop *binop);
static enum ir_state emit_instr_phi(struct ir_context *ctx, struct mir_instr_phi *phi);
static enum ir_state emit_instr_set_initializer(struct ir_context *ctx, struct mir_instr_set_initializer *si);
static enum ir_state emit_instr_type_info(struct ir_context *ctx, struct mir_instr_type_info *type_info);
static enum ir_state emit_instr_test_cases(struct ir_context *ctx, struct mir_instr_test_case *tc);
static enum ir_state emit_instr_decl_ref(struct ir_context *ctx, struct mir_instr_decl_ref *ref);
static enum ir_state emit_instr_decl_direct_ref(struct ir_context *ctx, struct mir_instr_decl_direct_ref *ref);
static enum ir_state emit_instr_cast(struct ir_context *ctx, struct mir_instr_cast *cast);
static enum ir_state emit_instr_addrof(struct ir_context *ctx, struct mir_instr_addrof *addrof);
static enum ir_state emit_instr_unop(struct ir_context *ctx, struct mir_instr_unop *unop);
static enum ir_state emit_instr_unreachable(struct ir_context *ctx, struct mir_instr_unreachable *unr);
static enum ir_state emit_instr_debugbreak(struct ir_context *ctx, struct mir_instr_debugbreak *debug_break);
static enum ir_state emit_instr_store(struct ir_context *ctx, struct mir_instr_store *store);
static enum ir_state emit_instr_fn_proto(struct ir_context *ctx, struct mir_instr_fn_proto *fn_proto);
static enum ir_state emit_instr_block(struct ir_context *ctx, struct mir_instr_block *block);
static enum ir_state emit_instr_br(struct ir_context *ctx, struct mir_instr_br *br);
static enum ir_state emit_instr_switch(struct ir_context *ctx, struct mir_instr_switch *sw);
static enum ir_state emit_instr_const(struct ir_context *ctx, struct mir_instr_const *c);
static enum ir_state emit_instr_arg(struct ir_context *ctx, struct mir_var *dest, struct mir_instr_arg *arg);
static enum ir_state emit_instr_cond_br(struct ir_context *ctx, struct mir_instr_cond_br *br);
static enum ir_state emit_instr_ret(struct ir_context *ctx, struct mir_instr_ret *ret);
static enum ir_state emit_instr_decl_var(struct ir_context *ctx, struct mir_instr_decl_var *decl);
static enum ir_state emit_instr_load(struct ir_context *ctx, struct mir_instr_load *load);
static enum ir_state emit_instr_call(struct ir_context *ctx, struct mir_instr_call *call);
static enum ir_state emit_instr_elem_ptr(struct ir_context *ctx, struct mir_instr_elem_ptr *elem_ptr);
static enum ir_state emit_instr_member_ptr(struct ir_context *ctx, struct mir_instr_member_ptr *member_ptr);
static enum ir_state emit_instr_unroll(struct ir_context *ctx, struct mir_instr_unroll *unroll);
static enum ir_state emit_instr_vargs(struct ir_context *ctx, struct mir_instr_vargs *vargs);
static enum ir_state emit_instr_toany(struct ir_context *ctx, struct mir_instr_to_any *toany);
static enum ir_state emit_instr_call_loc(struct ir_context *ctx, struct mir_instr_call_loc *loc);
static LLVMValueRef  emit_global_var_proto(struct ir_context *ctx, struct mir_var *var);
static LLVMValueRef  emit_fn_proto(struct ir_context *ctx, struct mir_fn *fn, bool schedule_full_generation);
static void          emit_allocas(struct ir_context *ctx, struct mir_fn *fn);
static void          emit_incomplete(struct ir_context *ctx);
static void          emit_instr_compound(struct ir_context *ctx, LLVMValueRef llvm_dest, struct mir_instr_compound *cmp);
static LLVMValueRef  _emit_instr_compound_zero_initialized(struct ir_context         *ctx,
                                                           LLVMValueRef               llvm_dest, // optional
                                                           struct mir_instr_compound *cmp);
static LLVMValueRef  _emit_instr_compound_comptime(struct ir_context *ctx, struct mir_instr_compound *cmp);

static inline LLVMValueRef emit_instr_compound_global(struct ir_context *ctx, struct mir_instr_compound *cmp) {
	bassert(mir_is_global(&cmp->base) && "Expected global compound expression!");
	bassert(mir_is_comptime(&cmp->base) && "Expected compile time known compound expression!");
	bassert(!cmp->is_naked && "Global compound expression cannot be naked!");
	return _emit_instr_compound_comptime(ctx, cmp);
}

static inline LLVMTypeRef get_type(struct ir_context *ctx, struct mir_type *t) {
	bassert(t);
	bassert(t->llvm_type && "Invalid type reference for LLVM!");
	bassert(t->kind != MIR_TYPE_INVALID);
	bassert(t->kind != MIR_TYPE_TYPE);
	bassert(t->kind != MIR_TYPE_FN_GROUP);
	bassert(t->kind != MIR_TYPE_NAMED_SCOPE);
	bassert(t->kind != MIR_TYPE_PLACEHOLDER);
	if (ctx->generate_debug_info && !t->llvm_meta) {
		DI_type_init(ctx, t);
	}
	return t->llvm_type;
}

static enum ir_state emit_instr_compound_naked(struct ir_context *ctx, struct mir_instr_compound *cmp) {
	bassert(cmp->is_naked && "Expected naked compound initializer!");
	bassert(cmp->tmp_var && "Missing tmp variable for naked compound instruction!");
	bassert(!mir_is_global(&cmp->base) && "Global compound cannot be naked!");
	LLVMValueRef llvm_tmp = cmp->tmp_var->llvm_value;
	bassert(llvm_tmp && "Missing LLVM representation for compound tmp variable!");

	emit_instr_compound(ctx, llvm_tmp, cmp);
	cmp->base.llvm_value =
	    LLVMBuildLoad2(ctx->llvm_builder, get_type(ctx, cmp->base.value.type), llvm_tmp, "");
	return STATE_PASSED;
}

static inline void emit_DI_instr_loc(struct ir_context *ctx, struct mir_instr *instr) {
	bassert(instr && "Invalid instruction!");
	bassert(instr->node && "Invalid instruction ast node!");
	struct scope    *scope = instr->node->owner_scope;
	struct location *loc   = instr->node->location;
	bassert(scope && "Missing scope for DI!");
	bassert(loc && "Missing location for DI!");
	LLVMMetadataRef llvm_scope =
	    !scope_is_local(scope) ? DI_unit_init(ctx, loc->unit) : DI_scope_init(ctx, scope);
	bassert(llvm_scope && "Missing DI scope!");
	LLVMMetadataRef llvm_loc = llvm_di_builder_create_debug_location(ctx->llvm_cnt, loc->line, 0, llvm_scope, NULL);
	LLVMSetCurrentDebugLocation2(ctx->llvm_builder, llvm_loc);
}

static inline LLVMValueRef llvm_lookup_fn(struct ir_context *ctx, const str_t name) {
	zone();
	const hash_t hash  = strhash(name);
	const s64    index = tbl_lookup_index_with_key(ctx->llvm_fn_cache, hash, name);
	if (index == -1) return_zone(NULL);
	return_zone(ctx->llvm_fn_cache[index].value);
}

static inline LLVMValueRef llvm_cache_fn(struct ir_context *ctx, str_t name, LLVMValueRef llvm_fn) {
	zone();
	const hash_t       hash  = strhash(name);
	struct cache_entry entry = {
	    .hash  = hash,
	    .key   = name,
	    .value = llvm_fn,
	};
	tbl_insert(ctx->llvm_fn_cache, entry);
	return_zone(llvm_fn);
}

static inline bool is_initialized(LLVMValueRef constant) {
	return constant && LLVMGetInitializer(constant);
}

static inline LLVMBasicBlockRef emit_basic_block(struct ir_context *ctx, struct mir_instr_block *block) {
	if (!block) return NULL;
	LLVMBasicBlockRef llvm_block = NULL;
	if (!block->base.llvm_value) {
		llvm_block             = llvm_append_basic_block_in_context(ctx->llvm_cnt, block->owner_fn->llvm_value, block->name);
		block->base.llvm_value = LLVMBasicBlockAsValue(llvm_block);
	} else {
		llvm_block = LLVMValueAsBasicBlock(block->base.llvm_value);
	}
	return llvm_block;
}

static void process_queue(struct ir_context *ctx) {
	struct mir_instr *instr;
	while (qmaybeswap(&ctx->queue)) {
		instr = qpop_front(&ctx->queue);
		bassert(instr);
		emit_instr(ctx, instr);
	}
}

str_t ir_get_intrinsic(const str_t name) {
	if (!name.len) return str_empty;
	if (str_match(name, cstr("sin.f32"))) return cstr("llvm.sin.f32");
	if (str_match(name, cstr("sin.f64"))) return cstr("llvm.sin.f64");
	if (str_match(name, cstr("cos.f32"))) return cstr("llvm.cos.f32");
	if (str_match(name, cstr("cos.f64"))) return cstr("llvm.cos.f64");
	if (str_match(name, cstr("pow.f32"))) return cstr("llvm.pow.f32");
	if (str_match(name, cstr("pow.f64"))) return cstr("llvm.pow.f64");
	if (str_match(name, cstr("exp.f32"))) return cstr("llvm.exp.f32");
	if (str_match(name, cstr("exp.f64"))) return cstr("llvm.exp.f64");
	if (str_match(name, cstr("log.f32"))) return cstr("llvm.log.f32");
	if (str_match(name, cstr("log.f64"))) return cstr("llvm.log.f64");
	if (str_match(name, cstr("log2.f32"))) return cstr("llvm.log2.f32");
	if (str_match(name, cstr("log2.f64"))) return cstr("llvm.log2.f64");
	if (str_match(name, cstr("sqrt.f32"))) return cstr("llvm.sqrt.f32");
	if (str_match(name, cstr("sqrt.f64"))) return cstr("llvm.sqrt.f64");
	if (str_match(name, cstr("ceil.f32"))) return cstr("llvm.ceil.f32");
	if (str_match(name, cstr("ceil.f64"))) return cstr("llvm.ceil.f64");
	if (str_match(name, cstr("round.f32"))) return cstr("llvm.round.f32");
	if (str_match(name, cstr("round.f64"))) return cstr("llvm.round.f64");
	if (str_match(name, cstr("floor.f32"))) return cstr("llvm.floor.f32");
	if (str_match(name, cstr("floor.f64"))) return cstr("llvm.floor.f64");
	if (str_match(name, cstr("log10.f32"))) return cstr("llvm.log10.f32");
	if (str_match(name, cstr("log10.f64"))) return cstr("llvm.log10.f64");
	if (str_match(name, cstr("trunc.f32"))) return cstr("llvm.trunc.f32");
	if (str_match(name, cstr("trunc.f64"))) return cstr("llvm.trunc.f64");
	if (str_match(name, cstr("memmove.p0.p0.i64"))) return cstr("llvm.memmove.p0.p0.i64");

	return str_empty;
}

// impl
LLVMMetadataRef DI_type_init(struct ir_context *ctx, struct mir_type *type) {
	if (type->llvm_meta) return type->llvm_meta;
	const str_t name = type->user_id ? type->user_id->str : type->id.str;

	switch (type->kind) {
	case MIR_TYPE_INT: {
		LLVMDWARFTypeEncoding encoding;

		if (type->data.integer.is_signed) {
			if (type->size_bits == 8)
				encoding = DW_ATE_signed_char;
			else
				encoding = DW_ATE_signed;
		} else {
			if (type->size_bits == 8)
				// Use signed char here to get readable string representations.
				encoding = DW_ATE_signed_char;
			else
				encoding = DW_ATE_unsigned;
		}
		type->llvm_meta = LLVMDIBuilderCreateBasicType(ctx->llvm_di_builder,
		                                               name.ptr,
		                                               (uint64_t)name.len,
		                                               (unsigned int)type->size_bits,
		                                               encoding,
		                                               LLVMDIFlagZero);
		break;
	}

	case MIR_TYPE_REAL: {
		type->llvm_meta = LLVMDIBuilderCreateBasicType(ctx->llvm_di_builder,
		                                               name.ptr,
		                                               (uint64_t)name.len,
		                                               (unsigned int)type->size_bits,
		                                               DW_ATE_float,
		                                               LLVMDIFlagZero);
		break;
	}

	case MIR_TYPE_PTR: {
		struct mir_type *tmp = mir_deref_type(type);
		type->llvm_meta      = LLVMDIBuilderCreatePointerType(ctx->llvm_di_builder,
                                                         DI_type_init(ctx, tmp),
                                                         type->size_bits,
                                                         (u32)type->alignment * 8,
                                                         0,
                                                         name.ptr,
                                                         (uint64_t)name.len);
		break;
	}

	case MIR_TYPE_VOID: {
		type->llvm_meta = LLVMDIBuilderCreateBasicType(
		    ctx->llvm_di_builder, "void", 4, 8, DW_ATE_unsigned_char, LLVMDIFlagZero);
		break;
	}

	case MIR_TYPE_NULL: {
		type->llvm_meta = LLVMDIBuilderCreateNullPtrType(ctx->llvm_di_builder);
		break;
	}

	case MIR_TYPE_BOOL: {
		type->llvm_meta = LLVMDIBuilderCreateBasicType(
		    ctx->llvm_di_builder, name.ptr, (uint64_t)name.len, 8, DW_ATE_boolean, LLVMDIFlagZero);
		break;
	}

	case MIR_TYPE_ARRAY: {
		// @Incomplete: Check this https://dwarfstd.org/doc/dwarf_1_1_0.pdf
		LLVMMetadataRef llvm_subrange =
		    LLVMDIBuilderGetOrCreateSubrange(ctx->llvm_di_builder, 0, type->data.array.len);
		type->llvm_meta =
		    LLVMDIBuilderCreateArrayType(ctx->llvm_di_builder,
		                                 (u64)type->data.array.len,
		                                 (u32)type->alignment * 8,
		                                 DI_type_init(ctx, type->data.array.elem_type),
		                                 &llvm_subrange,
		                                 1);
		break;
	}

	case MIR_TYPE_STRING:
	case MIR_TYPE_VARGS:
	case MIR_TYPE_SLICE:
	case MIR_TYPE_DYNARR:
	case MIR_TYPE_STRUCT: {
		// Struct type will be generated as forward declaration and postponed to be filled
		// later. This approach solves problems with circular references.
		type->llvm_meta = LLVMDIBuilderCreateReplaceableCompositeType(
		    ctx->llvm_di_builder, 0, "", 0, NULL, NULL, 0, 0, 0, 0, LLVMDIFlagZero, "", 0);

		arrput(ctx->di_incomplete_types, type);
		break;
	}

	case MIR_TYPE_ENUM: {
		LLVMMetadataRef scope_meta, file_meta;
		bassert(type->data.enm.scope && "Missing enum scope!");
		const struct location *location = type->data.enm.scope->location;
		scope_meta                      = DI_scope_init(ctx, type->data.enm.scope->parent);
		bassert(scope_meta && "Missing scope LLVM metadata!");
		if (location) {
			file_meta = DI_unit_init(ctx, location->unit);
		} else {
			// This applies for builtin types without source location.
			file_meta = scope_meta;
		}
		struct mir_type *base_type  = type->data.enm.base_type;
		const str_t      enm_name   = type->user_id ? type->user_id->str : cstr("enum");
		llvm_metas_t     llvm_elems = SARR_ZERO;
		mir_variants_t  *variants   = type->data.enm.variants;
		for (usize i = 0; i < sarrlenu(variants); ++i) {
			struct mir_variant *variant = sarrpeek(variants, i);
			const struct id     id      = *variant->id;
			LLVMMetadataRef     llvm_variant =
			    LLVMDIBuilderCreateEnumerator(ctx->llvm_di_builder,
			                                  id.str.ptr,
			                                  id.str.len,
			                                  (s64)variant->value,
			                                  !base_type->data.integer.is_signed);
			sarrput(&llvm_elems, llvm_variant);
		}
		type->llvm_meta =
		    LLVMDIBuilderCreateEnumerationType(ctx->llvm_di_builder,
		                                       scope_meta,
		                                       enm_name.ptr,
		                                       enm_name.len,
		                                       file_meta,
		                                       location ? (unsigned)location->line : 0,
		                                       type->size_bits,
		                                       (u32)type->alignment * 8,
		                                       sarrdata(&llvm_elems),
		                                       (u32)sarrlenu(&llvm_elems),
		                                       DI_type_init(ctx, base_type));
		sarrfree(&llvm_elems);
		break;
	}

	case MIR_TYPE_FN: {
		llvm_metas_t params = SARR_ZERO;
		// return type is first
		sarrput(&params, DI_type_init(ctx, type->data.fn.ret_type));
		for (usize i = 0; i < sarrlenu(type->data.fn.args); ++i) {
			struct mir_arg *it = sarrpeek(type->data.fn.args, i);
			// No debug info for comptime arguments.
			if (isflag(it->flags, FLAG_COMPTIME)) continue;
			sarrput(&params, DI_type_init(ctx, it->type));
		}
		// @Incomplete: file meta not used?
		type->llvm_meta = LLVMDIBuilderCreateSubroutineType(
		    ctx->llvm_di_builder, NULL, sarrdata(&params), (u32)sarrlenu(&params), LLVMDIFlagZero);

		sarrfree(&params);
		break;
	}

	default: {
		str_buf_t type_name = mir_type2str(type, true);
		babort("Missing generation DI for type '%s'.", str_buf_to_c(type_name));
		// Leak type_name we're aborting...
	}
	}
	bassert(type->llvm_meta);
	return type->llvm_meta;
}

LLVMMetadataRef DI_complete_type(struct ir_context *ctx, struct mir_type *type) {
	bassert(type->llvm_meta && "Incomplete DI type must have forward declaration.");

	switch (type->kind) {
	case MIR_TYPE_STRING:
	case MIR_TYPE_VARGS:
	case MIR_TYPE_SLICE:
	case MIR_TYPE_DYNARR:
	case MIR_TYPE_STRUCT: {
		bassert(type->llvm_meta && "Missing composit type fwd Di decl!!!");
		const bool      is_union    = type->data.strct.is_union;
		const bool      is_implicit = !type->data.strct.scope->location;
		LLVMMetadataRef llvm_file;
		unsigned        struct_line;

		if (is_implicit) {
			struct_line = 0;
			llvm_file   = ctx->assembly->gscope->llvm_meta;
		} else {
			struct location *location = type->data.strct.scope->location;
			llvm_file                 = DI_unit_init(ctx, location->unit);
			struct_line               = (unsigned)location->line;
		}

		const LLVMMetadataRef llvm_scope = type->llvm_meta;

		bassert(llvm_file);
		bassert(llvm_scope);
		str_t struct_name = cstr("<implicit_struct>");
		if (type->user_id) {
			struct_name = type->user_id->str;
		} else {
			// Use type signature in case there is no user type.
			struct_name = type->id.str;
		}

		llvm_metas_t   llvm_elems = SARR_ZERO;
		mir_members_t *members    = type->data.strct.members;
		for (usize i = 0; i < sarrlenu(members); ++i) {
			struct mir_member *elem      = sarrpeek(members, i);
			unsigned           elem_line = elem->decl_node ? (unsigned)elem->decl_node->location->line : 0;
			LLVMMetadataRef    llvm_elem = LLVMDIBuilderCreateMemberType(
                ctx->llvm_di_builder,
                llvm_scope,
                elem->id->str.ptr,
                (uint64_t)elem->id->str.len,
                llvm_file,
                elem_line,
                elem->type->size_bits,
                (unsigned)elem->type->alignment * 8,
                (unsigned)(vm_get_struct_elem_offset(ctx->assembly, type, (u32)i) * 8),
                LLVMDIFlagZero,
                DI_type_init(ctx, elem->type));

			sarrput(&llvm_elems, llvm_elem);
		}

		LLVMMetadataRef llvm_parent_scope = llvm_file;

		LLVMMetadataRef llvm_struct;
		if (is_union) {
			llvm_struct = LLVMDIBuilderCreateUnionType(ctx->llvm_di_builder,
			                                           llvm_parent_scope,
			                                           struct_name.ptr,
			                                           (uint64_t)struct_name.len,
			                                           llvm_file,
			                                           struct_line,
			                                           type->size_bits,
			                                           (unsigned)type->alignment * 8,
			                                           LLVMDIFlagZero,
			                                           sarrdata(&llvm_elems),
			                                           (u32)sarrlenu(&llvm_elems),
			                                           0,
			                                           "",
			                                           0);
		} else {
			llvm_struct = LLVMDIBuilderCreateStructType(ctx->llvm_di_builder,
			                                            llvm_parent_scope,
			                                            struct_name.ptr,
			                                            (uint64_t)struct_name.len,
			                                            llvm_file,
			                                            struct_line,
			                                            type->size_bits,
			                                            (unsigned)type->alignment * 8,
			                                            LLVMDIFlagZero,
			                                            NULL,
			                                            sarrdata(&llvm_elems),
			                                            (u32)sarrlenu(&llvm_elems),
			                                            0,
			                                            NULL,
			                                            "",
			                                            0);
		}
		LLVMMetadataReplaceAllUsesWith(type->llvm_meta, llvm_struct);
		type->llvm_meta = llvm_struct;

		sarrfree(&llvm_elems);
		break;
	}

	default: {
		babort("Missing DI completion for type %d", type->kind);
	}
	}

	return type->llvm_meta;
}

LLVMMetadataRef DI_scope_init(struct ir_context *ctx, struct scope *scope) {
	bassert(scope && "Invalid scope!");
	if (scope->llvm_meta) return scope->llvm_meta;
	switch (scope->kind) {
	case SCOPE_LEXICAL: {
		bassert(scope->location);
		LLVMMetadataRef llvm_parent_scope = DI_scope_init(ctx, scope->parent);
		LLVMMetadataRef llvm_unit         = DI_unit_init(ctx, scope->location->unit);
		bassert(llvm_parent_scope);
		bassert(llvm_unit);
		scope->llvm_meta = LLVMDIBuilderCreateLexicalBlock(
		    ctx->llvm_di_builder, llvm_parent_scope, llvm_unit, (u32)scope->location->line, 0);
		break;
	}
	default:
		// Use global scope as fallback if there is no other option!
		// babort("Unsupported scope '%s' for DI generation", scope_kind_name(scope));
		bassert(ctx->assembly->gscope);
		scope->llvm_meta = ctx->assembly->gscope->llvm_meta;
		break;
	}
	bassert(scope->llvm_meta);
	return scope->llvm_meta;
}

LLVMMetadataRef DI_unit_init(struct ir_context *ctx, struct unit *unit) {
	if (unit->llvm_file_meta) return unit->llvm_file_meta;
	unit->llvm_file_meta = LLVMDIBuilderCreateFile(ctx->llvm_di_builder, unit->filename.ptr, unit->filename.len, unit->dirpath.ptr, unit->dirpath.len);
	return unit->llvm_file_meta;
}

void emit_DI_fn(struct ir_context *ctx, struct mir_fn *fn) {
	if (!fn->decl_node) return;
	bassert(fn->body_scope);
	if (fn->body_scope->llvm_meta) return;
	// We use file scope for debug info even for global functions to prevent some problems with
	// DWARF generation, caused mainly by putting subprogram info into lexical scopes i.e. in case
	// local function is generated.
	struct location *location  = fn->decl_node->location;
	LLVMMetadataRef  llvm_file = DI_unit_init(ctx, location->unit);
	bassert(llvm_file && "Missing DI file scope data!");
	const str_t name          = fn->id ? fn->id->str : fn->linkage_name;
	const str_t linkage_name  = fn->linkage_name;
	const bool  is_optimized  = ctx->assembly->target->opt != ASSEMBLY_OPT_DEBUG;
	fn->body_scope->llvm_meta = LLVMDIBuilderCreateFunction(ctx->llvm_di_builder,
	                                                        llvm_file,
	                                                        name.ptr,
	                                                        (uint64_t)name.len,
	                                                        linkage_name.ptr,
	                                                        (uint64_t)linkage_name.len,
	                                                        llvm_file,
	                                                        (u32)location->line,
	                                                        fn->type->llvm_meta,
	                                                        false,
	                                                        true,
	                                                        (u32)location->line,
	                                                        LLVMDIFlagPrototyped,
	                                                        is_optimized);
	LLVMSetSubprogram(fn->llvm_value, fn->body_scope->llvm_meta);
}

void emit_DI_var(struct ir_context *ctx, struct mir_var *var) {
	bassert(var->decl_node && "Variable has no declaration node!");
	bassert(var->id && "Variable has no id!");
	bassert(isnotflag(var->iflags, MIR_VAR_IMPLICIT) &&
	        "Attempt to generate debug info for implicit variable!");
	struct location *location = var->decl_node->location;
	bassert(location);
	if (isflag(var->iflags, MIR_VAR_GLOBAL)) {
		const str_t     name         = var->id->str;
		const str_t     linkage_name = var->linkage_name;
		LLVMMetadataRef llvm_scope   = DI_unit_init(ctx, location->unit);
		LLVMMetadataRef llvm_expr    = LLVMDIBuilderCreateExpression(ctx->llvm_di_builder, NULL, 0);
		LLVMMetadataRef llvm_meta =
		    LLVMDIBuilderCreateGlobalVariableExpression(ctx->llvm_di_builder,
		                                                llvm_scope,
		                                                name.ptr,
		                                                (uint64_t)name.len,
		                                                linkage_name.ptr,
		                                                (uint64_t)linkage_name.len,
		                                                llvm_scope,
		                                                (u32)location->line,
		                                                DI_type_init(ctx, var->value.type),
		                                                false,
		                                                llvm_expr,
		                                                NULL,
		                                                (u32)var->value.type->alignment * 8);

		LLVMGlobalSetMetadata(var->llvm_value, llvm_get_md_kind_id_in_context(ctx->llvm_cnt, cstr("dbg")), llvm_meta);
	} else { // Local variable
		bassert(location->unit->llvm_file_meta);
		LLVMMetadataRef llvm_scope = DI_scope_init(ctx, var->decl_scope);
		LLVMMetadataRef llvm_file  = DI_unit_init(ctx, location->unit);
		const str_t     name       = var->id->str;

		bassert(llvm_file && "Invalid DI file for variable DI!");
		bassert(llvm_scope && "Invalid DI scope for variable DI!");
		bassert(var->llvm_value);

		LLVMMetadataRef llvm_meta =
		    LLVMDIBuilderCreateAutoVariable(ctx->llvm_di_builder,
		                                    llvm_scope,
		                                    name.ptr,
		                                    (uint64_t)name.len,
		                                    llvm_file,
		                                    (u32)location->line,
		                                    DI_type_init(ctx, var->value.type),
		                                    false,
		                                    LLVMDIFlagZero,
		                                    (u32)var->value.type->alignment);

		LLVMMetadataRef   llvm_expr         = LLVMDIBuilderCreateExpression(ctx->llvm_di_builder, NULL, 0);
		LLVMMetadataRef   llvm_loc          = llvm_di_builder_create_debug_location(ctx->llvm_cnt, location->line, 0, llvm_scope, NULL);
		LLVMBasicBlockRef llvm_insert_block = LLVMGetInsertBlock(ctx->llvm_builder);
		LLVMDIBuilderInsertDeclareAtEnd(ctx->llvm_di_builder,
		                                var->llvm_value,
		                                llvm_meta,
		                                llvm_expr,
		                                llvm_loc,
		                                llvm_insert_block);
	}
}

LLVMValueRef emit_global_var_proto(struct ir_context *ctx, struct mir_var *var) {
	bassert(var);
	if (var->llvm_value) return var->llvm_value;
	// Push initializer.
	struct mir_instr *instr_init = var->initializer_block;
	bassert(instr_init && "Missing initializer block reference for IR global variable!");
	qpush_back(&ctx->queue, instr_init);
	LLVMTypeRef llvm_type = var->value.type->llvm_type;
	var->llvm_value       = llvm_add_global(ctx->llvm_module, llvm_type, var->linkage_name);
	LLVMSetGlobalConstant(var->llvm_value, isnotflag(var->iflags, MIR_VAR_MUTABLE));
	// Linkage should be later set by user.
	LLVMSetLinkage(var->llvm_value, LLVMPrivateLinkage);
	LLVMSetAlignment(var->llvm_value, (unsigned)var->value.type->alignment);
	if (isflag(var->flags, FLAG_THREAD_LOCAL)) {
		LLVMSetThreadLocalMode(var->llvm_value, LLVMGeneralDynamicTLSModel);
	}
	const bool emit_DI =
	    ctx->generate_debug_info && isnotflag(var->iflags, MIR_VAR_IMPLICIT) && var->decl_node;
	if (emit_DI) emit_DI_var(ctx, var);
	return var->llvm_value;
}

LLVMValueRef emit_fn_proto(struct ir_context *ctx, struct mir_fn *fn, bool schedule_full_generation) {
	bassert(fn);
	str_t linkage_name = str_empty;
	if (isflag(fn->flags, FLAG_INTRINSIC)) {
		linkage_name = ir_get_intrinsic(fn->linkage_name);
		bassert(linkage_name.len && "Unknown LLVM intrinsic!");
	} else {
		linkage_name = fn->linkage_name;
	}

	bassert(linkage_name.len && "Invalid function name!");
	fn->llvm_value = llvm_lookup_fn(ctx, linkage_name);
	if (fn->llvm_value) return fn->llvm_value;
	fn->llvm_value =
	    llvm_cache_fn(ctx,
	                  linkage_name,
	                  llvm_add_function(ctx->llvm_module, linkage_name, get_type(ctx, fn->type)));

	if (schedule_full_generation) {
		// Push function prototype instruction into generation queue.
		struct mir_instr *instr_fn_proto = fn->prototype;
		bassert(instr_fn_proto && "Missing function prototype!");
		qpush_back(&ctx->queue, instr_fn_proto);
	}

	// Setup attributes for sret.
	if (fn->type->data.fn.has_sret) {
		LLVMAddAttributeAtIndex(fn->llvm_value,
		                        LLVM_SRET_INDEX + 1,
		                        llvm_create_enum_attribute(ctx->llvm_cnt, LLVM_ATTR_NOALIAS, 0));

		struct mir_type *sret_arg_type = fn->type->data.fn.ret_type;
		LLVMAddAttributeAtIndex(fn->llvm_value,
		                        LLVM_SRET_INDEX + 1,
		                        llvm_create_type_attribute(ctx->llvm_cnt,
		                                                   LLVM_ATTR_STRUCTRET,
		                                                   get_type(ctx, sret_arg_type)));
		LLVMAddAttributeAtIndex(fn->llvm_value,
		                        LLVM_SRET_INDEX + 1,
		                        llvm_create_enum_attribute(ctx->llvm_cnt,
		                                                   LLVM_ATTR_ALIGNMENT,
		                                                   (u64)sret_arg_type->alignment));
	}
	// Setup attributes for byval.
	if (fn->type->data.fn.has_byval) {
		mir_args_t *args = fn->type->data.fn.args;
		bassert(args);
		for (usize i = 0; i < sarrlenu(args); ++i) {
			struct mir_arg *arg = sarrpeek(args, i);
			if (arg->llvm_easgm != LLVM_EASGM_BYVAL) continue;
			LLVMTypeRef llvm_arg_type = get_type(ctx, arg->type);

			LLVMAddAttributeAtIndex(
			    fn->llvm_value,
			    arg->llvm_index + 1,
			    llvm_create_type_attribute(ctx->llvm_cnt, LLVM_ATTR_BYVAL, llvm_arg_type));

			LLVMAddAttributeAtIndex(
			    fn->llvm_value,
			    arg->llvm_index + 1,
			    llvm_create_enum_attribute(ctx->llvm_cnt,
			                               LLVM_ATTR_ALIGNMENT,
			                               (u64)ctx->builtin_types->t_u8_ptr->alignment));
		}
	}
	if (isflag(fn->flags, FLAG_INLINE)) {
		bassert(!isflag(fn->flags, FLAG_NO_INLINE));
		LLVMAttributeRef llvm_attr = llvm_create_enum_attribute(ctx->llvm_cnt, LLVM_ATTR_ALWAYSINLINE, 0);
		LLVMAddAttributeAtIndex(fn->llvm_value, (unsigned)LLVMAttributeFunctionIndex, llvm_attr);
	}
	if (isflag(fn->flags, FLAG_NO_INLINE)) {
		bassert(!isflag(fn->flags, FLAG_INLINE));
		LLVMAttributeRef llvm_attr = llvm_create_enum_attribute(ctx->llvm_cnt, LLVM_ATTR_NOINLINE, 0);
		LLVMAddAttributeAtIndex(fn->llvm_value, (unsigned)LLVMAttributeFunctionIndex, llvm_attr);
	}
	if (isflag(fn->flags, FLAG_EXPORT)) {
		bassert(fn->is_global && "Exported function is supposed to be global!");
		LLVMSetVisibility(fn->llvm_value, LLVMDefaultVisibility);
		LLVMSetDLLStorageClass(fn->llvm_value, LLVMDLLExportStorageClass);
	} else if (isnotflag(fn->flags, FLAG_EXTERN) && isnotflag(fn->flags, FLAG_INTRINSIC)) {
		LLVMSetVisibility(fn->llvm_value, LLVMHiddenVisibility);
	}
	return fn->llvm_value;
}

LLVMValueRef emit_const_string(struct ir_context *ctx, const str_t s) {
	zone();
	struct mir_type *type     = ctx->builtin_types->t_string_literal;
	LLVMValueRef     llvm_str = NULL;
	if (s.len) {
		s64    index = -1;
		hash_t hash;

		const bool use_cache = s.len < 256;
		// There is probably no good reason to try cache large string data.
		if (use_cache) {
			hash  = strhash(s);
			index = tbl_lookup_index_with_key(ctx->gstring_cache, hash, s);
		}
		if (index != -1) {
			llvm_str = ctx->gstring_cache[index].value;
		} else {
			LLVMValueRef llvm_str_content = llvm_const_string_in_context(ctx->llvm_cnt, s, false);

			llvm_str = LLVMAddGlobal(ctx->llvm_module, LLVMTypeOf(llvm_str_content), ".str");
			LLVMSetInitializer(llvm_str, llvm_str_content);
			LLVMSetLinkage(llvm_str, LLVMPrivateLinkage);
			LLVMSetGlobalConstant(llvm_str, true);

			if (use_cache) {
				struct cache_entry entry = {
				    .hash  = hash,
				    .key   = s,
				    .value = llvm_str,
				};
				tbl_insert(ctx->gstring_cache, entry);
			}
		}
	} else {
		// null string content
		struct mir_type *str_type = mir_get_struct_elem_type(type, 1);
		llvm_str                  = LLVMConstNull(get_type(ctx, str_type));
	}

	LLVMValueRef values[2];

	struct mir_type *len_type = mir_get_struct_elem_type(type, 0);
	values[0]                 = LLVMConstInt(get_type(ctx, len_type), (uint64_t)s.len, true);
	values[1]                 = llvm_str;
	return_zone(LLVMConstNamedStruct(get_type(ctx, type), values, static_arrlenu(values)));
}

enum ir_state emit_instr_decl_ref(struct ir_context *ctx, struct mir_instr_decl_ref *ref) {
	struct scope_entry *entry = ref->scope_entry;
	bassert(entry);
	switch (entry->kind) {
	case SCOPE_ENTRY_VAR: {
		struct mir_var *var = entry->data.var;
		if (isflag(var->iflags, MIR_VAR_GLOBAL)) {
			ref->base.llvm_value = emit_global_var_proto(ctx, var);
		} else {
			ref->base.llvm_value = var->llvm_value;
		}
		break;
	}
	case SCOPE_ENTRY_FN: {
		ref->base.llvm_value = emit_fn_proto(ctx, entry->data.fn, true);
		break;
	}
	default:
		BL_UNIMPLEMENTED;
	}
	bassert(ref->base.llvm_value);
	return STATE_PASSED;
}

enum ir_state emit_instr_decl_direct_ref(struct ir_context *ctx, struct mir_instr_decl_direct_ref *ref) {
	bassert(ref->ref && ref->ref->kind == MIR_INSTR_DECL_VAR);
	struct mir_var *var = ((struct mir_instr_decl_var *)ref->ref)->var;
	bassert(var);
	if (isflag(var->iflags, MIR_VAR_GLOBAL)) {
		ref->base.llvm_value = emit_global_var_proto(ctx, var);
	} else {
		ref->base.llvm_value = var->llvm_value;
	}
	bassert(ref->base.llvm_value);
	return STATE_PASSED;
}

enum ir_state emit_instr_phi(struct ir_context *ctx, struct mir_instr_phi *phi) {
	const s32 count = phi->num;
	bassert(count > 0);
	if (count == 1) {
		// 2024-08-07 We have only one income, but phi itself was not replaced by constant, because the
		// income value is runtime.
		struct mir_instr *value = phi->incoming_values[0];
		bassert(value);
		bassert(value->llvm_value);
		phi->base.llvm_value = value->llvm_value;
		return STATE_PASSED;
	}

	llvm_values_t llvm_iv = SARR_ZERO;
	llvm_values_t llvm_ib = SARR_ZERO;
	for (s32 i = 0; i < count; ++i) {
		struct mir_instr *value = phi->incoming_values[i];
		bassert(value);
		struct mir_instr_block *block = phi->incoming_blocks[i];
		bassert(block);
		bassert(value->llvm_value);
		sarrput(&llvm_iv, value->llvm_value);
		sarrput(&llvm_ib, LLVMBasicBlockAsValue(emit_basic_block(ctx, block)));
	}
	LLVMValueRef llvm_phi =
	    LLVMBuildPhi(ctx->llvm_builder, get_type(ctx, phi->base.value.type), "");
	LLVMAddIncoming(
	    llvm_phi, sarrdata(&llvm_iv), (LLVMBasicBlockRef *)sarrdata(&llvm_ib), (unsigned int)count);
	sarrfree(&llvm_iv);
	sarrfree(&llvm_ib);
	phi->base.llvm_value = llvm_phi;
	return STATE_PASSED;
}

enum ir_state emit_instr_debugbreak(struct ir_context *ctx, struct mir_instr_debugbreak *debug_break) {
	struct mir_fn *break_fn = debug_break->break_fn;
	bassert(break_fn);
	if (!break_fn->llvm_value) emit_fn_proto(ctx, break_fn, true);
	DI_LOCATION_SET(&debug_break->base);
	LLVMBuildCall2(
	    ctx->llvm_builder, get_type(ctx, break_fn->type), break_fn->llvm_value, NULL, 0, "");
	DI_LOCATION_RESET();
	return STATE_PASSED;
}

enum ir_state emit_instr_unreachable(struct ir_context *ctx, struct mir_instr_unreachable *unreachable) {
	struct mir_fn *abort_fn = unreachable->abort_fn;
	bassert(abort_fn);
	if (!abort_fn->llvm_value) emit_fn_proto(ctx, abort_fn, true);
	DI_LOCATION_SET(&unreachable->base);
	LLVMBuildCall2(
	    ctx->llvm_builder, get_type(ctx, abort_fn->type), abort_fn->llvm_value, NULL, 0, "");
	DI_LOCATION_RESET();
	return STATE_PASSED;
}

// @Cleanup: we can eventually pass the whole type here instead of size and alignment.
LLVMValueRef rtti_emit_base(struct ir_context *ctx,
                            struct mir_type   *type,
                            enum mir_type_kind kind,
                            usize              size,
                            s8                 alignment) {
	LLVMValueRef     llvm_vals[3];
	struct mir_type *kind_type      = mir_get_struct_elem_type(type, 0);
	llvm_vals[0]                    = LLVMConstInt(get_type(ctx, kind_type), (u64)kind, false);
	struct mir_type *size_type      = mir_get_struct_elem_type(type, 1);
	llvm_vals[1]                    = LLVMConstInt(get_type(ctx, size_type), size, false);
	struct mir_type *alignment_type = mir_get_struct_elem_type(type, 2);
	llvm_vals[2]                    = LLVMConstInt(get_type(ctx, alignment_type), (u64)alignment, false);
	return LLVMConstNamedStruct(get_type(ctx, type), llvm_vals, static_arrlenu(llvm_vals));
}

LLVMValueRef rtti_emit_empty(struct ir_context *ctx, struct mir_type *type, struct mir_type *rtti_type) {
	struct mir_type *base_type = mir_get_struct_elem_type(rtti_type, 0);
	LLVMValueRef     llvm_val =
	    rtti_emit_base(ctx, base_type, type->kind, type->store_size_bytes, type->alignment);
	return LLVMConstNamedStruct(get_type(ctx, rtti_type), &llvm_val, 1);
}

LLVMValueRef rtti_emit_enum(struct ir_context *ctx, struct mir_type *type) {
	struct mir_type *rtti_type = ctx->builtin_types->t_TypeInfoEnum;
	LLVMValueRef     llvm_vals[5];

	// base
	struct mir_type *base_type = mir_get_struct_elem_type(rtti_type, 0);
	llvm_vals[0] =
	    rtti_emit_base(ctx, base_type, type->kind, type->store_size_bytes, type->alignment);

	// name
	const str_t name = type->user_id ? type->user_id->str : type->id.str;
	llvm_vals[1]     = emit_const_string(ctx, name);

	// base_type
	llvm_vals[2] = _rtti_emit(ctx, type->data.enm.base_type);

	// variants
	llvm_vals[3] = rtti_emit_enum_variants_slice(ctx, type->data.enm.variants);

	// is_flags
	const bool       is_flags      = type->data.enm.is_flags;
	struct mir_type *is_flags_type = mir_get_struct_elem_type(rtti_type, 4);
	llvm_vals[4]                   = LLVMConstInt(
        get_type(ctx, is_flags_type), (u64)is_flags, is_flags_type->data.integer.is_signed);

	return LLVMConstNamedStruct(get_type(ctx, rtti_type), llvm_vals, static_arrlenu(llvm_vals));
}

LLVMValueRef rtti_emit_enum_variant(struct ir_context *ctx, struct mir_variant *variant) {
	struct mir_type *rtti_type = ctx->builtin_types->t_TypeInfoEnumVariant;
	LLVMValueRef     llvm_vals[2];
	// name
	llvm_vals[0] = emit_const_string(ctx, variant->id->str);

	// value
	struct mir_type *value_type = mir_get_struct_elem_type(rtti_type, 1);

	bassert(variant->value_type);
	bassert(variant->value_type->kind == MIR_TYPE_ENUM);
	struct mir_type *base_enum_type = variant->value_type->data.enm.base_type;

	// The variant value is stored in the type info structure as an unsigned integer, but an
	// enumerator base type can be a signed integer, so we have to do propper casting here to keep
	// negative values negative!
	LLVMValueRef llvm_temporary_value = LLVMConstInt(
	    get_type(ctx, base_enum_type), variant->value, base_enum_type->data.integer.is_signed);

	llvm_vals[1] = LLVMBuildCast(
	    ctx->llvm_builder, LLVMSExt, llvm_temporary_value, get_type(ctx, value_type), "");

	return LLVMConstNamedStruct(get_type(ctx, rtti_type), llvm_vals, static_arrlenu(llvm_vals));
}

LLVMValueRef rtti_emit_enum_variants_array(struct ir_context *ctx, mir_variants_t *variants) {
	struct mir_type *elem_type = ctx->builtin_types->t_TypeInfoEnumVariant;
	llvm_values_t    llvm_vals = SARR_ZERO;
	for (usize i = 0; i < sarrlenu(variants); ++i) {
		struct mir_variant *it = sarrpeek(variants, i);
		sarrput(&llvm_vals, rtti_emit_enum_variant(ctx, it));
	}
	LLVMValueRef llvm_result =
	    LLVMConstArray(get_type(ctx, elem_type), sarrdata(&llvm_vals), (u32)sarrlenu(&llvm_vals));

	LLVMValueRef llvm_rtti_var =
	    LLVMAddGlobal(ctx->llvm_module, LLVMTypeOf(llvm_result), ".rtti_variants");
	LLVMSetLinkage(llvm_rtti_var, LLVMPrivateLinkage);
	LLVMSetGlobalConstant(llvm_rtti_var, true);
	LLVMSetInitializer(llvm_rtti_var, llvm_result);

	sarrfree(&llvm_vals);
	return llvm_rtti_var;
}

LLVMValueRef rtti_emit_enum_variants_slice(struct ir_context *ctx, mir_variants_t *variants) {
	struct mir_type *type = ctx->builtin_types->t_TypeInfoEnumVariants_slice;
	LLVMValueRef     llvm_vals[2];
	struct mir_type *len_type = mir_get_struct_elem_type(type, 0);
	llvm_vals[0]              = LLVMConstInt(
        get_type(ctx, len_type), (u32)sarrlenu(variants), len_type->data.integer.is_signed);
	llvm_vals[1] = rtti_emit_enum_variants_array(ctx, variants);
	return LLVMConstNamedStruct(get_type(ctx, type), llvm_vals, static_arrlenu(llvm_vals));
}

LLVMValueRef rtti_emit_struct(struct ir_context *ctx, struct mir_type *type) {
	struct mir_type *rtti_type = ctx->builtin_types->t_TypeInfoStruct;
	LLVMValueRef     llvm_vals[6];

	// base
	struct mir_type *base_type = mir_get_struct_elem_type(rtti_type, 0);
	llvm_vals[0] =
	    rtti_emit_base(ctx, base_type, MIR_TYPE_STRUCT, type->store_size_bytes, type->alignment);

	// name
	const str_t name = type->user_id ? type->user_id->str : type->id.str;
	llvm_vals[1]     = emit_const_string(ctx, name);

	// members
	llvm_vals[2] = rtti_emit_struct_members_slice(ctx, type->data.strct.members);

	// is_slice
	const bool       is_slice      = type->kind == MIR_TYPE_SLICE || type->kind == MIR_TYPE_VARGS;
	struct mir_type *is_slice_type = mir_get_struct_elem_type(rtti_type, 3);
	llvm_vals[3]                   = LLVMConstInt(
        get_type(ctx, is_slice_type), (u64)is_slice, is_slice_type->data.integer.is_signed);

	// is_union
	const bool       is_union      = type->data.strct.is_union;
	struct mir_type *is_union_type = mir_get_struct_elem_type(rtti_type, 4);
	llvm_vals[4]                   = LLVMConstInt(
        get_type(ctx, is_union_type), (u64)is_union, is_union_type->data.integer.is_signed);

	// is_dynamic_array
	const bool       is_da      = type->kind == MIR_TYPE_DYNARR;
	struct mir_type *is_da_type = mir_get_struct_elem_type(rtti_type, 5);
	llvm_vals[5] =
	    LLVMConstInt(get_type(ctx, is_da_type), (u64)is_da, is_union_type->data.integer.is_signed);

	return LLVMConstNamedStruct(get_type(ctx, rtti_type), llvm_vals, static_arrlenu(llvm_vals));
}

LLVMValueRef rtti_emit_struct_member(struct ir_context *ctx, struct mir_member *member) {
	struct mir_type *rtti_type = ctx->builtin_types->t_TypeInfoStructMember;
	LLVMValueRef     llvm_vals[6];

	// name
	llvm_vals[0] = emit_const_string(ctx, member->id->str);

	// base_type
	llvm_vals[1] = _rtti_emit(ctx, member->type);

	// offset_bytes
	struct mir_type *offset_type = mir_get_struct_elem_type(rtti_type, 2);
	llvm_vals[2]                 = LLVMConstInt(
        get_type(ctx, offset_type), (u32)member->offset_bytes, offset_type->data.integer.is_signed);

	// index
	struct mir_type *index_type = mir_get_struct_elem_type(rtti_type, 3);
	llvm_vals[3]                = LLVMConstInt(
        get_type(ctx, offset_type), (u32)member->index, index_type->data.integer.is_signed);

	// tags
	struct mir_type *tag_type = mir_get_struct_elem_type(rtti_type, 4);
	llvm_vals[4] =
	    LLVMConstInt(get_type(ctx, tag_type), member->tag, tag_type->data.integer.is_signed);

	// is_base
	struct mir_type *is_base_type = mir_get_struct_elem_type(rtti_type, 5);
	llvm_vals[5]                  = LLVMConstInt(
        get_type(ctx, is_base_type), (u32)member->is_base, is_base_type->data.integer.is_signed);
	return LLVMConstNamedStruct(get_type(ctx, rtti_type), llvm_vals, static_arrlenu(llvm_vals));
}

LLVMValueRef rtti_emit_struct_members_array(struct ir_context *ctx, mir_members_t *members) {
	struct mir_type *elem_type = ctx->builtin_types->t_TypeInfoStructMember;
	llvm_values_t    llvm_vals = SARR_ZERO;

	for (usize i = 0; i < sarrlenu(members); ++i) {
		struct mir_member *it = sarrpeek(members, i);
		sarrput(&llvm_vals, rtti_emit_struct_member(ctx, it));
	}

	LLVMValueRef llvm_result =
	    LLVMConstArray(get_type(ctx, elem_type), sarrdata(&llvm_vals), (u32)sarrlenu(&llvm_vals));

	LLVMValueRef llvm_rtti_var =
	    LLVMAddGlobal(ctx->llvm_module, LLVMTypeOf(llvm_result), ".rtti_members");
	LLVMSetLinkage(llvm_rtti_var, LLVMPrivateLinkage);
	LLVMSetGlobalConstant(llvm_rtti_var, true);
	LLVMSetInitializer(llvm_rtti_var, llvm_result);

	sarrfree(&llvm_vals);
	return llvm_rtti_var;
}

LLVMValueRef rtti_emit_struct_members_slice(struct ir_context *ctx, mir_members_t *members) {
	struct mir_type *type = ctx->builtin_types->t_TypeInfoStructMembers_slice;
	LLVMValueRef     llvm_vals[2];
	struct mir_type *len_type = mir_get_struct_elem_type(type, 0);

	llvm_vals[0] = LLVMConstInt(
	    get_type(ctx, len_type), (u32)sarrlenu(members), len_type->data.integer.is_signed);
	llvm_vals[1] = rtti_emit_struct_members_array(ctx, members);
	return LLVMConstNamedStruct(get_type(ctx, type), llvm_vals, static_arrlenu(llvm_vals));
}

LLVMValueRef rtti_emit_fn(struct ir_context *ctx, struct mir_type *type) {
	struct mir_type *rtti_type = ctx->builtin_types->t_TypeInfoFn;
	LLVMValueRef     llvm_vals[4];

	// base
	struct mir_type *base_type = mir_get_struct_elem_type(rtti_type, 0);
	llvm_vals[0] =
	    rtti_emit_base(ctx, base_type, type->kind, type->store_size_bytes, type->alignment);

	// args
	llvm_vals[1] = rtti_emit_fn_args_slice(ctx, type->data.fn.args);

	// ret_type
	llvm_vals[2] = _rtti_emit(ctx, type->data.fn.ret_type);

	// is_vargs
	struct mir_type *is_vargs_type = mir_get_struct_elem_type(rtti_type, 3);
	const bool       is_vargs      = type->data.fn.is_vargs;
	llvm_vals[3]                   = LLVMConstInt(
        get_type(ctx, is_vargs_type), (u64)is_vargs, is_vargs_type->data.integer.is_signed);
	return LLVMConstNamedStruct(get_type(ctx, rtti_type), llvm_vals, static_arrlenu(llvm_vals));
}

LLVMValueRef rtti_emit_fn_group(struct ir_context *ctx, struct mir_type *type) {
	struct mir_type *rtti_type = ctx->builtin_types->t_TypeInfoFnGroup;
	LLVMValueRef     llvm_vals[2];
	// base
	struct mir_type *base_type = mir_get_struct_elem_type(rtti_type, 0);
	llvm_vals[0] =
	    rtti_emit_base(ctx, base_type, type->kind, type->store_size_bytes, type->alignment);

	// variants
	llvm_vals[1] = rtti_emit_fn_slice(ctx, type->data.fn_group.variants);
	return LLVMConstNamedStruct(get_type(ctx, rtti_type), llvm_vals, static_arrlenu(llvm_vals));
}

LLVMValueRef rtti_emit_fn_slice(struct ir_context *ctx, mir_types_t *fns) {
	struct mir_type *type = ctx->builtin_types->t_TypeInfoFn_ptr_slice;
	LLVMValueRef     llvm_vals[2];
	const usize      argc     = sarrlenu(fns);
	struct mir_type *len_type = mir_get_struct_elem_type(type, 0);
	struct mir_type *ptr_type = mir_get_struct_elem_type(type, 1);
	llvm_vals[0]              = LLVMConstInt(get_type(ctx, len_type), argc, len_type->data.integer.is_signed);
	if (argc) {
		llvm_vals[1] = rtti_emit_fn_array(ctx, fns);
	} else {
		llvm_vals[1] = LLVMConstNull(get_type(ctx, ptr_type));
	}
	return LLVMConstNamedStruct(get_type(ctx, type), llvm_vals, static_arrlenu(llvm_vals));
}

LLVMValueRef rtti_emit_fn_array(struct ir_context *ctx, mir_types_t *fns) {
	struct mir_type *elem_type = ctx->builtin_types->t_TypeInfoFn_ptr;
	llvm_values_t    llvm_vals = SARR_ZERO;

	for (usize i = 0; i < sarrlenu(fns); ++i) {
		struct mir_type *it = sarrpeek(fns, i);
		sarrput(&llvm_vals, _rtti_emit(ctx, it));
	}

	LLVMValueRef llvm_result =
	    LLVMConstArray(get_type(ctx, elem_type), sarrdata(&llvm_vals), (u32)sarrlenu(&llvm_vals));

	LLVMValueRef llvm_rtti_var =
	    LLVMAddGlobal(ctx->llvm_module, LLVMTypeOf(llvm_result), ".rtti_args");
	LLVMSetLinkage(llvm_rtti_var, LLVMPrivateLinkage);
	LLVMSetGlobalConstant(llvm_rtti_var, true);
	LLVMSetInitializer(llvm_rtti_var, llvm_result);
	sarrfree(&llvm_vals);
	return llvm_rtti_var;
}

LLVMValueRef rtti_emit_fn_arg(struct ir_context *ctx, struct mir_arg *arg) {
	struct mir_type *rtti_type = ctx->builtin_types->t_TypeInfoFnArg;
	LLVMValueRef     llvm_vals[2];
	// name
	const str_t arg_name = arg->id ? arg->id->str : str_empty;
	llvm_vals[0]         = emit_const_string(ctx, arg_name);
	// base_type
	llvm_vals[1] = _rtti_emit(ctx, arg->type);
	return LLVMConstNamedStruct(get_type(ctx, rtti_type), llvm_vals, static_arrlenu(llvm_vals));
}

LLVMValueRef rtti_emit_fn_args_array(struct ir_context *ctx, mir_args_t *args) {
	struct mir_type *elem_type = ctx->builtin_types->t_TypeInfoFnArg;
	llvm_values_t    llvm_vals = SARR_ZERO;

	for (usize i = 0; i < sarrlenu(args); ++i) {
		struct mir_arg *it = sarrpeek(args, i);
		sarrput(&llvm_vals, rtti_emit_fn_arg(ctx, it));
	}

	LLVMValueRef llvm_result =
	    LLVMConstArray(get_type(ctx, elem_type), sarrdata(&llvm_vals), (u32)sarrlenu(&llvm_vals));

	LLVMValueRef llvm_rtti_var =
	    LLVMAddGlobal(ctx->llvm_module, LLVMTypeOf(llvm_result), ".rtti_args");
	LLVMSetLinkage(llvm_rtti_var, LLVMPrivateLinkage);
	LLVMSetGlobalConstant(llvm_rtti_var, true);
	LLVMSetInitializer(llvm_rtti_var, llvm_result);

	sarrfree(&llvm_vals);
	return llvm_rtti_var;
}

LLVMValueRef rtti_emit_fn_args_slice(struct ir_context *ctx, mir_args_t *args) {
	struct mir_type *type = ctx->builtin_types->t_TypeInfoFnArgs_slice;
	LLVMValueRef     llvm_vals[2];
	const usize      argc = sarrlenu(args);

	struct mir_type *len_type = mir_get_struct_elem_type(type, 0);
	struct mir_type *ptr_type = mir_get_struct_elem_type(type, 1);

	llvm_vals[0] = LLVMConstInt(get_type(ctx, len_type), argc, len_type->data.integer.is_signed);

	if (argc) {
		llvm_vals[1] = rtti_emit_fn_args_array(ctx, args);
	} else {
		llvm_vals[1] = LLVMConstNull(get_type(ctx, ptr_type));
	}
	return LLVMConstNamedStruct(get_type(ctx, type), llvm_vals, static_arrlenu(llvm_vals));
}

LLVMValueRef rtti_emit_integer(struct ir_context *ctx, struct mir_type *type) {
	struct mir_type *rtti_type = ctx->builtin_types->t_TypeInfoInt;
	LLVMValueRef     llvm_vals[3];

	struct mir_type *base_type = mir_get_struct_elem_type(rtti_type, 0);
	llvm_vals[0] =
	    rtti_emit_base(ctx, base_type, type->kind, type->store_size_bytes, type->alignment);

	struct mir_type *bitcount_type = mir_get_struct_elem_type(rtti_type, 1);
	llvm_vals[1] =
	    LLVMConstInt(get_type(ctx, bitcount_type), (u32)type->data.integer.bitcount, true);

	struct mir_type *is_signed_type = mir_get_struct_elem_type(rtti_type, 2);
	llvm_vals[2] =
	    LLVMConstInt(get_type(ctx, is_signed_type), (u32)type->data.integer.is_signed, true);
	return LLVMConstNamedStruct(get_type(ctx, rtti_type), llvm_vals, static_arrlenu(llvm_vals));
}

LLVMValueRef rtti_emit_real(struct ir_context *ctx, struct mir_type *type) {
	struct mir_type *rtti_type = ctx->builtin_types->t_TypeInfoReal;
	LLVMValueRef     llvm_vals[2];

	struct mir_type *base_type = mir_get_struct_elem_type(rtti_type, 0);
	llvm_vals[0] =
	    rtti_emit_base(ctx, base_type, type->kind, type->store_size_bytes, type->alignment);

	struct mir_type *bitcount_type = mir_get_struct_elem_type(rtti_type, 1);
	llvm_vals[1] =
	    LLVMConstInt(get_type(ctx, bitcount_type), (u32)type->data.integer.bitcount, true);
	return LLVMConstNamedStruct(get_type(ctx, rtti_type), llvm_vals, static_arrlenu(llvm_vals));
}

LLVMValueRef rtti_emit_ptr(struct ir_context *ctx, struct mir_type *type) {
	struct mir_type *rtti_type = ctx->builtin_types->t_TypeInfoPtr;
	LLVMValueRef     llvm_vals[2];

	struct mir_type *base_type = mir_get_struct_elem_type(rtti_type, 0);
	llvm_vals[0] =
	    rtti_emit_base(ctx, base_type, type->kind, type->store_size_bytes, type->alignment);

	// pointee
	llvm_vals[1] = _rtti_emit(ctx, type->data.ptr.expr);

	return LLVMConstNamedStruct(get_type(ctx, rtti_type), llvm_vals, static_arrlenu(llvm_vals));
}

LLVMValueRef rtti_emit_array(struct ir_context *ctx, struct mir_type *type) {
	struct mir_type *rtti_type = ctx->builtin_types->t_TypeInfoArray;
	LLVMValueRef     llvm_vals[4];

	struct mir_type *base_type = mir_get_struct_elem_type(rtti_type, 0);
	llvm_vals[0] =
	    rtti_emit_base(ctx, base_type, type->kind, type->store_size_bytes, type->alignment);

	// name
	const str_t name = type->user_id ? type->user_id->str : type->id.str;
	llvm_vals[1]     = emit_const_string(ctx, name);

	// elem_type
	llvm_vals[2] = _rtti_emit(ctx, type->data.array.elem_type);

	// len
	struct mir_type *len_type = mir_get_struct_elem_type(rtti_type, 3);
	llvm_vals[3]              = LLVMConstInt(
        get_type(ctx, len_type), (u32)type->data.array.len, len_type->data.integer.is_signed);

	return LLVMConstNamedStruct(get_type(ctx, rtti_type), llvm_vals, static_arrlenu(llvm_vals));
}

LLVMValueRef rtti_emit(struct ir_context *ctx, struct mir_type *type) {
	LLVMValueRef llvm_value = _rtti_emit(ctx, type);

	while (arrlen(ctx->incomplete_rtti)) {
		rtti_satisfy_incomplete(ctx, arrpop(ctx->incomplete_rtti));
	}

	return llvm_value;
}

void rtti_satisfy_incomplete(struct ir_context *ctx, struct rtti_incomplete incomplete) {
	struct mir_type *type          = incomplete.type;
	LLVMValueRef     llvm_rtti_var = incomplete.llvm_var;

	bassert(type->kind == MIR_TYPE_PTR);
	LLVMValueRef llvm_value = rtti_emit_ptr(ctx, type);

	bassert(llvm_value);
	LLVMSetInitializer(llvm_rtti_var, llvm_value);
}

LLVMValueRef _rtti_emit(struct ir_context *ctx, struct mir_type *type) {
	bassert(type);

	struct mir_var *rtti_var = mir_get_rtti(ctx->assembly, type->id.hash);
	bassert(rtti_var && "RTTI variable not generated for the type!");
	if (rtti_var->llvm_value) {
		return rtti_var->llvm_value;
	}

	LLVMValueRef llvm_rtti_var = llvm_add_global(
	    ctx->llvm_module, get_type(ctx, rtti_var->value.type), rtti_var->linkage_name);
	LLVMSetLinkage(llvm_rtti_var, LLVMPrivateLinkage);
	LLVMSetGlobalConstant(llvm_rtti_var, true);

	LLVMValueRef llvm_value = NULL;

	switch (type->kind) {
	case MIR_TYPE_INT:
		llvm_value = rtti_emit_integer(ctx, type);
		break;

	case MIR_TYPE_ENUM:
		llvm_value = rtti_emit_enum(ctx, type);
		break;

	case MIR_TYPE_REAL:
		llvm_value = rtti_emit_real(ctx, type);
		break;

	case MIR_TYPE_BOOL:
		llvm_value = rtti_emit_empty(ctx, type, ctx->builtin_types->t_TypeInfoBool);
		break;

	case MIR_TYPE_TYPE:
		llvm_value = rtti_emit_empty(ctx, type, ctx->builtin_types->t_TypeInfoType);
		break;

	case MIR_TYPE_VOID:
		llvm_value = rtti_emit_empty(ctx, type, ctx->builtin_types->t_TypeInfoVoid);
		break;

	case MIR_TYPE_NULL:
		llvm_value = rtti_emit_empty(ctx, type, ctx->builtin_types->t_TypeInfoNull);
		break;

	case MIR_TYPE_STRING:
		llvm_value = rtti_emit_empty(ctx, type, ctx->builtin_types->t_TypeInfoString);
		break;

	case MIR_TYPE_PTR:
		arrput(ctx->incomplete_rtti,
		       ((struct rtti_incomplete){.llvm_var = llvm_rtti_var, .type = type}));
		goto SKIP;

	case MIR_TYPE_ARRAY:
		llvm_value = rtti_emit_array(ctx, type);
		break;

	case MIR_TYPE_DYNARR:
	case MIR_TYPE_SLICE:
	case MIR_TYPE_VARGS:
	case MIR_TYPE_STRUCT:
		llvm_value = rtti_emit_struct(ctx, type);
		break;

	case MIR_TYPE_FN:
		llvm_value = rtti_emit_fn(ctx, type);
		break;

	case MIR_TYPE_FN_GROUP:
		llvm_value = rtti_emit_fn_group(ctx, type);
		break;

	default: {
		str_buf_t type_name = mir_type2str(type, true);
		babort("Missing LLVM RTTI generation for type '%s'", str_buf_to_c(type_name));
		// no put_tstr here...
	}
	}

	bassert(llvm_value);

	LLVMSetInitializer(llvm_rtti_var, llvm_value);

SKIP:
	rtti_var->llvm_value = llvm_rtti_var;
	return llvm_rtti_var;
}

enum ir_state emit_instr_type_info(struct ir_context *ctx, struct mir_instr_type_info *type_info) {
	bassert(type_info->rtti_type);
	type_info->base.llvm_value = rtti_emit(ctx, type_info->rtti_type);
	return STATE_PASSED;
}

LLVMValueRef testing_emit_meta_case(struct ir_context *ctx, struct mir_fn *fn) {
	struct mir_type *type = ctx->builtin_types->t_TestCase;
	LLVMValueRef     llvm_vals[4];

	const str_t filename = fn->decl_node ? fn->decl_node->location->unit->filename : cstr("UNKNOWN");
	const s32   line     = fn->decl_node ? fn->decl_node->location->line : 0;

	llvm_vals[0] = emit_fn_proto(ctx, fn, true);
	llvm_vals[1] = emit_const_string(ctx, fn->id->str);
	llvm_vals[2] = emit_const_string(ctx, filename);
	llvm_vals[3] = LLVMConstInt(get_type(ctx, mir_get_struct_elem_type(type, 3)), (u64)line, true);
	return LLVMConstNamedStruct(get_type(ctx, type), llvm_vals, static_arrlenu(llvm_vals));
}

struct mir_var *testing_fetch_meta(struct ir_context *ctx) {
	struct mir_var *var = ctx->assembly->testing.meta_var;
	if (!var) return NULL;

	bassert(var->value.type && var->value.type->kind == MIR_TYPE_ARRAY);
	const s64 len = var->value.type->data.array.len;
	bassert(len == arrlen(ctx->assembly->testing.cases));
	if (var->llvm_value) {
		return var;
	}

	// Emit global for storing test case informations only once and only if needed.
	LLVMTypeRef  llvm_var_type = get_type(ctx, var->value.type);
	LLVMValueRef llvm_var      = llvm_add_global(ctx->llvm_module, llvm_var_type, var->linkage_name);
	LLVMSetLinkage(llvm_var, LLVMPrivateLinkage);
	LLVMSetGlobalConstant(llvm_var, true);

	var->llvm_value = llvm_var;

	// Setup values
	llvm_values_t llvm_vals = SARR_ZERO;

	for (usize i = 0; i < arrlenu(ctx->assembly->testing.cases); ++i) {
		struct mir_fn *fn = ctx->assembly->testing.cases[i];
		sarrput(&llvm_vals, testing_emit_meta_case(ctx, fn));
	}

	LLVMValueRef llvm_init = LLVMConstArray(
	    get_type(ctx, ctx->builtin_types->t_TestCase), sarrdata(&llvm_vals), (unsigned int)len);

	LLVMSetInitializer(llvm_var, llvm_init);
	sarrfree(&llvm_vals);
	return var;
}

enum ir_state emit_instr_test_cases(struct ir_context *ctx, struct mir_instr_test_case *tc) {
	// Test case metadata variable is optional an can be null in case there are no test cases.
	// In such case we generate empty slice with zero length.
	struct mir_var  *var  = testing_fetch_meta(ctx);
	struct mir_type *type = ctx->builtin_types->t_TestCases_slice;

	LLVMValueRef llvm_vals[2];

	struct mir_type *len_type = mir_get_struct_elem_type(type, 0);
	struct mir_type *ptr_type = mir_get_struct_elem_type(type, 1);

	LLVMValueRef llvm_len =
	    var ? LLVMConstInt(get_type(ctx, len_type),
	                       (u64)var->value.type->data.array.len,
	                       len_type->data.integer.is_signed)
	        : LLVMConstInt(get_type(ctx, len_type), 0, len_type->data.integer.is_signed);

	LLVMValueRef llvm_ptr = var ? var->llvm_value : LLVMConstPointerNull(get_type(ctx, ptr_type));

	llvm_vals[0] = llvm_len;
	llvm_vals[1] = llvm_ptr;

	tc->base.llvm_value =
	    LLVMConstNamedStruct(get_type(ctx, type), llvm_vals, static_arrlenu(llvm_vals));
	return STATE_PASSED;
}

enum ir_state emit_instr_cast(struct ir_context *ctx, struct mir_instr_cast *cast) {
	LLVMValueRef llvm_src       = cast->expr->llvm_value;
	LLVMTypeRef  llvm_dest_type = get_type(ctx, cast->base.value.type);
	LLVMOpcode   llvm_op;
	bassert(llvm_src && llvm_dest_type);

	switch (cast->op) {
	case MIR_CAST_NONE:
		cast->base.llvm_value = llvm_src;
		return STATE_PASSED;
	case MIR_CAST_BITCAST:
		// bitcast is noop since LLVM 15
		cast->base.llvm_value = llvm_src;
		return STATE_PASSED;

	case MIR_CAST_SEXT:
		llvm_op = LLVMSExt;
		break;

	case MIR_CAST_ZEXT:
		llvm_op = LLVMZExt;
		break;

	case MIR_CAST_TRUNC:
		llvm_op = LLVMTrunc;
		break;

	case MIR_CAST_FPTOSI:
		llvm_op = LLVMFPToSI;
		break;

	case MIR_CAST_FPTOUI:
		llvm_op = LLVMFPToUI;
		break;

	case MIR_CAST_FPTRUNC:
		llvm_op = LLVMFPTrunc;
		break;

	case MIR_CAST_FPEXT:
		llvm_op = LLVMFPExt;
		break;

	case MIR_CAST_SITOFP:
		llvm_op = LLVMSIToFP;
		break;

	case MIR_CAST_UITOFP:
		llvm_op = LLVMUIToFP;
		break;

	case MIR_CAST_PTRTOINT:
		llvm_op = LLVMPtrToInt;
		break;

	case MIR_CAST_INTTOPTR:
		llvm_op = LLVMIntToPtr;
		break;

	case MIR_CAST_PTRTOBOOL: {
		// Cast from pointer to bool has no internal LLVM representation so we have
		// to emulate it here by simple comparison pointer value to it's null
		// equivalent.
		LLVMTypeRef  llvm_src_type = get_type(ctx, cast->expr->value.type);
		LLVMValueRef llvm_null     = LLVMConstNull(llvm_src_type);
		cast->base.llvm_value =
		    LLVMBuildICmp(ctx->llvm_builder, LLVMIntNE, llvm_src, llvm_null, "");
		return STATE_PASSED;
	}

	default:
		babort("invalid cast type");
	}

	cast->base.llvm_value = LLVMBuildCast(ctx->llvm_builder, llvm_op, llvm_src, llvm_dest_type, "");
	return STATE_PASSED;
}

enum ir_state emit_instr_addrof(struct ir_context *ctx, struct mir_instr_addrof *addrof) {
	if (addrof->src->kind == MIR_INSTR_FN_PROTO) {
		struct mir_instr_fn_proto *fn_proto = (struct mir_instr_fn_proto *)addrof->src;
		struct mir_fn             *fn       = MIR_CEV_READ_AS(struct mir_fn *, &fn_proto->base.value);
		bmagic_assert(fn);
		addrof->base.llvm_value = emit_fn_proto(ctx, fn, true);
	} else {
		addrof->base.llvm_value = addrof->src->llvm_value;
	}
	bassert(addrof->base.llvm_value);
	return STATE_PASSED;
}

enum ir_state emit_instr_arg(struct ir_context *ctx, struct mir_var *dest, struct mir_instr_arg *arg_instr) {
	bassert(dest);
	struct mir_fn *fn = arg_instr->base.owner_block->owner_fn;
	bassert(fn);
	struct mir_type *fn_type = fn->type;
	LLVMValueRef     llvm_fn = fn->llvm_value;
	bassert(llvm_fn);

	struct mir_arg *arg = sarrpeek(fn_type->data.fn.args, arg_instr->i);
	bassert(isnotflag(arg->flags, FLAG_COMPTIME) && "Comptime arguments should be evaluated and replaced by constants!");
	LLVMValueRef llvm_dest = dest->llvm_value;
	bassert(llvm_dest);

	switch (arg->llvm_easgm) {
	case LLVM_EASGM_NONE: { // Default. Arg value unchanged.
		LLVMValueRef llvm_arg = LLVMGetParam(llvm_fn, arg->llvm_index);
		LLVMBuildStore(ctx->llvm_builder, llvm_arg, llvm_dest);
		break;
	}

	case LLVM_EASGM_8:
	case LLVM_EASGM_16:
	case LLVM_EASGM_32:
	case LLVM_EASGM_64: {
		LLVMValueRef llvm_arg = LLVMGetParam(llvm_fn, arg->llvm_index);
		llvm_dest             = LLVMBuildBitCast(
            ctx->llvm_builder, llvm_dest, LLVMPointerType(LLVMTypeOf(llvm_arg), 0), "");

		LLVMBuildStore(ctx->llvm_builder, llvm_arg, llvm_dest);
		break;
	}

	case LLVM_EASGM_64_8:
	case LLVM_EASGM_64_16:
	case LLVM_EASGM_64_32:
	case LLVM_EASGM_64_64: {
		LLVMValueRef llvm_arg_1 = LLVMGetParam(llvm_fn, arg->llvm_index);
		LLVMValueRef llvm_arg_2 = LLVMGetParam(llvm_fn, arg->llvm_index + 1);

		LLVMTypeRef llvm_arg_elem_types[] = {LLVMTypeOf(llvm_arg_1), LLVMTypeOf(llvm_arg_2)};
		LLVMTypeRef llvm_arg_type         = llvm_struct_type_in_context(ctx->llvm_cnt, llvm_arg_elem_types, 2, false);

		llvm_dest =
		    LLVMBuildBitCast(ctx->llvm_builder, llvm_dest, LLVMPointerType(llvm_arg_type, 0), "");

		LLVMValueRef llvm_dest_1 =
		    LLVMBuildStructGEP2(ctx->llvm_builder, llvm_arg_type, llvm_dest, 0, "");
		LLVMBuildStore(ctx->llvm_builder, llvm_arg_1, llvm_dest_1);
		LLVMValueRef llvm_dest_2 =
		    LLVMBuildStructGEP2(ctx->llvm_builder, llvm_arg_type, llvm_dest, 1, "");
		LLVMBuildStore(ctx->llvm_builder, llvm_arg_2, llvm_dest_2);
		break;
	}

	case LLVM_EASGM_BYVAL: {
		LLVMValueRef llvm_arg = LLVMGetParam(llvm_fn, arg->llvm_index);
		build_call_memcpy(ctx, llvm_arg, llvm_dest, dest->value.type->store_size_bytes);
		break;
	}
	}

	arg_instr->base.llvm_value = NULL;
	return STATE_PASSED;
}

enum ir_state emit_instr_elem_ptr(struct ir_context *ctx, struct mir_instr_elem_ptr *elem_ptr) {
	struct mir_type *arr_type = mir_deref_type(elem_ptr->arr_ptr->value.type);
	bassert(arr_type);

	LLVMValueRef llvm_index = elem_ptr->index->llvm_value;
	bassert(llvm_index);

	const bool is_global = mir_is_global(&elem_ptr->base);

	switch (arr_type->kind) {
	case MIR_TYPE_ARRAY: {
		LLVMValueRef llvm_arr_ptr = elem_ptr->arr_ptr->llvm_value;
		LLVMValueRef llvm_indices[2];
		llvm_indices[0] = ctx->llvm_const_i64_zero;
		llvm_indices[1] = llvm_index;

		if (is_global) {
			bassert(LLVMIsConstant(llvm_arr_ptr) && "Expected constant!");
			elem_ptr->base.llvm_value = LLVMConstGEP2(
			    get_type(ctx, arr_type), llvm_arr_ptr, llvm_indices, static_arrlenu(llvm_indices));
		} else {
			// @Incomplete: Check the type!!!
			elem_ptr->base.llvm_value = LLVMBuildGEP2(ctx->llvm_builder,
			                                          get_type(ctx, arr_type),
			                                          llvm_arr_ptr,
			                                          llvm_indices,
			                                          static_arrlenu(llvm_indices),
			                                          "");
		}
		break;
	}

	case MIR_TYPE_DYNARR:
	case MIR_TYPE_SLICE:
	case MIR_TYPE_STRING:
	case MIR_TYPE_VARGS: {
		// In this case the input arr_ptr is slice (aka { len, ptr }). That means we have to extract
		// the ptr value first, then load it and then return pointer to the value at the offset from
		// loaded pointer (the same as access to array in C).

		LLVMValueRef llvm_slice_ptr = elem_ptr->arr_ptr->llvm_value;
		LLVMValueRef llvm_ptr_ptr   = LLVMBuildStructGEP2(
            ctx->llvm_builder, get_type(ctx, arr_type), llvm_slice_ptr, MIR_SLICE_PTR_INDEX, "");

		struct mir_type *elem_type = mir_get_struct_elem_type(arr_type, MIR_SLICE_PTR_INDEX);
		LLVMValueRef     llvm_ptr =
		    LLVMBuildLoad2(ctx->llvm_builder, get_type(ctx, elem_type), llvm_ptr_ptr, "");
		bassert(llvm_ptr);

		LLVMValueRef llvm_indices[1] = {llvm_index};

		// Note that the GEP for arrays expects the type to resolve size of each element in the
		// array, so another deref is needed here to get the actual elem type from pointer to the
		// array.
		struct mir_type *arr_indexing_type = mir_deref_type(elem_type);

		elem_ptr->base.llvm_value = LLVMBuildInBoundsGEP2(ctx->llvm_builder,
		                                                  get_type(ctx, arr_indexing_type),
		                                                  llvm_ptr,
		                                                  llvm_indices,
		                                                  static_arrlenu(llvm_indices),
		                                                  "");

		break;
	}

	default:
		babort("Invalid elem ptr target type!");
	}
	return STATE_PASSED;
}

enum ir_state emit_instr_member_ptr(struct ir_context *ctx, struct mir_instr_member_ptr *member_ptr) {
	LLVMValueRef llvm_target_ptr = member_ptr->target_ptr->llvm_value;
	bassert(llvm_target_ptr);

	if (member_ptr->builtin_id == BUILTIN_ID_NONE) {
		bassert(member_ptr->scope_entry->kind == SCOPE_ENTRY_MEMBER);
		struct mir_member *member = member_ptr->scope_entry->data.member;
		bassert(member);

		if (member->is_parent_union) {
			// Union's member produce bitcast only.
			member_ptr->base.llvm_value = llvm_target_ptr;
		} else {
			const unsigned int index       = (const unsigned int)member->index;
			struct mir_type   *target_type = mir_deref_type(member_ptr->target_ptr->value.type);
			bassert(target_type);
			LLVMTypeRef llvm_target_type = get_type(ctx, target_type);

			member_ptr->base.llvm_value = LLVMBuildStructGEP2(
			    ctx->llvm_builder, llvm_target_type, llvm_target_ptr, index, "");
		}

		bassert(member_ptr->base.llvm_value);
		return STATE_PASSED;
	}

	// builtin member

	// Valid only for slice types, we generate direct replacement for arrays.
	if (member_ptr->builtin_id == BUILTIN_ID_ARR_LEN) {
		// .len
		LLVMTypeRef llvm_type = get_type(
		    ctx, mir_get_struct_elem_type(member_ptr->target_ptr->value.type, MIR_SLICE_LEN_INDEX));
		member_ptr->base.llvm_value = LLVMBuildStructGEP2(
		    ctx->llvm_builder, llvm_type, llvm_target_ptr, MIR_SLICE_LEN_INDEX, "");
	} else if (member_ptr->builtin_id == BUILTIN_ID_ARR_PTR) {
		// .ptr
		LLVMTypeRef llvm_type = get_type(
		    ctx, mir_get_struct_elem_type(member_ptr->target_ptr->value.type, MIR_SLICE_PTR_INDEX));
		member_ptr->base.llvm_value = LLVMBuildStructGEP2(
		    ctx->llvm_builder, llvm_type, llvm_target_ptr, MIR_SLICE_PTR_INDEX, "");
	}
	return STATE_PASSED;
}

enum ir_state emit_instr_unroll(struct ir_context *ctx, struct mir_instr_unroll *unroll) {
	LLVMValueRef llvm_src_ptr = unroll->src->llvm_value;
	bassert(llvm_src_ptr);

	const unsigned int index = (const unsigned int)unroll->index;
	unroll->base.llvm_value =
	    LLVMBuildStructGEP2(ctx->llvm_builder,
	                        get_type(ctx, mir_deref_type(unroll->src->value.type)),
	                        llvm_src_ptr,
	                        index,
	                        "");
	return STATE_PASSED;
}

enum ir_state emit_instr_load(struct ir_context *ctx, struct mir_instr_load *load) {
	bassert(load->base.value.type && "invalid type of load instruction");
	LLVMValueRef llvm_src = load->src->llvm_value;
	bassert(llvm_src);

	// Check if we deal with global constant, in such case no load is needed, LLVM
	// global
	// constants are referenced by value, so we need to fetch initializer instead of
	// load. */
	if (mir_is_global(load->src)) {
		// When we try to create comptime constant composition and load value,
		// initializer is needed. But loaded value could be in incomplete state
		// during instruction emit, we need to postpone this instruction in such
		// case and try to resume later.
		if (!is_initialized(llvm_src)) {
			return STATE_POSTPONE;
		}
		load->base.llvm_value = LLVMGetInitializer(llvm_src);
		bassert(load->base.llvm_value);
		return STATE_PASSED;
	}
	DI_LOCATION_SET(&load->base);
	load->base.llvm_value =
	    LLVMBuildLoad2(ctx->llvm_builder, get_type(ctx, load->base.value.type), llvm_src, "");
	DI_LOCATION_RESET();
	const unsigned alignment = (const unsigned)load->base.value.type->alignment;
	LLVMSetAlignment(load->base.llvm_value, alignment);
	return STATE_PASSED;
}

static LLVMValueRef
build_call_memcpy(struct ir_context *ctx, LLVMValueRef src, LLVMValueRef dest, const usize size_bytes) {
	LLVMValueRef llvm_args[4];

	llvm_args[0] = dest;
	llvm_args[1] = src;
	llvm_args[2] = LLVMConstInt(ctx->builtin_types->t_u64->llvm_type, size_bytes, true);
	llvm_args[3] = LLVMConstInt(ctx->builtin_types->t_bool->llvm_type, 0, true);

	LLVMValueRef llvm_call = LLVMBuildCall2(ctx->llvm_builder,
	                                        ctx->intrinsic_memcpy_type,
	                                        ctx->intrinsic_memcpy,
	                                        llvm_args,
	                                        static_arrlenu(llvm_args),
	                                        "");

	return llvm_call;
}

static LLVMValueRef
build_optimized_store(struct ir_context *ctx, struct mir_instr *src, struct mir_instr *dest) {
	bassert(mir_is_pointer_type(dest->value.type));
	LLVMValueRef llvm_src   = src->llvm_value;
	LLVMValueRef llvm_dest  = dest->llvm_value;
	const usize  size_bytes = (usize)mir_deref_type(dest->value.type)->store_size_bytes;
	if (size_bytes > STORE_MAX_SIZE_BYTES && src->kind == MIR_INSTR_LOAD) {
		struct mir_instr_load *load = (struct mir_instr_load *)src;
		llvm_src                    = load->src->llvm_value;
		LLVMInstructionEraseFromParent(load->base.llvm_value);
		return build_call_memcpy(ctx, llvm_src, llvm_dest, size_bytes);
	}
	const unsigned alignment  = (unsigned)src->value.type->alignment;
	LLVMValueRef   llvm_store = LLVMBuildStore(ctx->llvm_builder, llvm_src, llvm_dest);
	LLVMSetAlignment(llvm_store, alignment);
	return llvm_store;
}

enum ir_state emit_instr_store(struct ir_context *ctx, struct mir_instr_store *store) {
	LLVMValueRef dest = store->dest->llvm_value;
	bassert(dest && "Missing LLVM store destination value!");
	if (store->src->kind == MIR_INSTR_COMPOUND) {
		emit_instr_compound(ctx, dest, (struct mir_instr_compound *)store->src);
		return STATE_PASSED;
	}
	DI_LOCATION_SET(&store->base);
	store->base.llvm_value = build_optimized_store(ctx, store->src, store->dest);
	DI_LOCATION_RESET();
	return STATE_PASSED;
}

enum ir_state emit_instr_unop(struct ir_context *ctx, struct mir_instr_unop *unop) {
	LLVMValueRef llvm_val = unop->expr->llvm_value;
	bassert(llvm_val);

	LLVMTypeKind lhs_kind   = LLVMGetTypeKind(LLVMTypeOf(llvm_val));
	const bool   float_kind = lhs_kind == LLVMFloatTypeKind || lhs_kind == LLVMDoubleTypeKind;

	switch (unop->op) {
	case UNOP_BIT_NOT:
	case UNOP_NOT: {
		bassert(!float_kind && "Invalid negation of floating point type.");
		unop->base.llvm_value = LLVMBuildNot(ctx->llvm_builder, llvm_val, "");
		break;
	}

	case UNOP_NEG: {
		if (float_kind)
			unop->base.llvm_value = LLVMBuildFNeg(ctx->llvm_builder, llvm_val, "");
		else
			unop->base.llvm_value = LLVMBuildNeg(ctx->llvm_builder, llvm_val, "");
		break;
	}

	case UNOP_POS: {
		unop->base.llvm_value = llvm_val;
		break;
	}

	default:
		BL_UNIMPLEMENTED;
	}
	return STATE_PASSED;
}

// Generates zero initialized value from compound expression, `llvm_dest` is optional
// specification of destination variable or GEP to be set to zero. Result LLVM value is either
// compile time constant or `llvm_dest` if provided.
LLVMValueRef _emit_instr_compound_zero_initialized(struct ir_context         *ctx,
                                                   LLVMValueRef               llvm_dest, // optional
                                                   struct mir_instr_compound *cmp) {
	struct mir_type *type = cmp->base.value.type;
	bassert(type);
	if (!llvm_dest) {
		cmp->base.llvm_value = LLVMConstNull(get_type(ctx, type));
		return cmp->base.llvm_value;
	}

	// Directly initialize destination variable!
	if (type->store_size_bytes <= ctx->builtin_types->t_u8_ptr->store_size_bytes) {
		// Small values use store instruction.
		cmp->base.llvm_value = LLVMConstNull(get_type(ctx, type));
		LLVMBuildStore(ctx->llvm_builder, cmp->base.llvm_value, llvm_dest);
	} else {
		bassert(!mir_is_global(&cmp->base) && "Cannot use memset for global zero initialization!");
		bassert(LLVMGetTypeKind(LLVMTypeOf(llvm_dest)) == LLVMPointerTypeKind);
		// Use memset intrinsic for zero initialization of variable
		LLVMValueRef args[4];
		args[0] = llvm_dest;
		args[1] = ctx->llvm_const_i8_zero;
		args[2] =
		    LLVMConstInt(get_type(ctx, ctx->builtin_types->t_u64), type->store_size_bytes, false);
		args[3] = LLVMConstInt(get_type(ctx, ctx->builtin_types->t_bool), 0, false);
		LLVMBuildCall2(ctx->llvm_builder,
		               ctx->intrinsic_memset_type,
		               ctx->intrinsic_memset,
		               args,
		               static_arrlenu(args),
		               "");
	}
	cmp->base.llvm_value = llvm_dest;
	return llvm_dest;
}

#define EMIT_NESTED_COMPOUND_IF_NEEDED(ctx, instr)                                \
	if (!(instr)->llvm_value) {                                                   \
		bassert((instr)->kind == MIR_INSTR_COMPOUND);                             \
		_emit_instr_compound_comptime(ctx, ((struct mir_instr_compound *)instr)); \
		bassert((instr)->llvm_value);                                             \
	}                                                                             \
	(void)0

// Generates comptime compound expression value, every nested member must be compile time known.
LLVMValueRef _emit_instr_compound_comptime(struct ir_context *ctx, struct mir_instr_compound *cmp) {
	bassert(mir_is_comptime(&cmp->base) && "Expected comptime known compound expression!");
	if (mir_is_zero_initialized(cmp)) {
		cmp->base.llvm_value = _emit_instr_compound_zero_initialized(ctx, NULL, cmp);
		bassert(cmp->base.llvm_value);
		return cmp->base.llvm_value;
	}

	struct mir_type *type = cmp->base.value.type;
	bassert(type);

	ints_t *mapping = cmp->value_member_mapping;

	switch (type->kind) {
	case MIR_TYPE_ARRAY: {
		bassert(mapping == NULL && "Mapping is currently not supported for arrays.");
		const u32   len            = (u32)type->data.array.len;
		LLVMTypeRef llvm_elem_type = get_type(ctx, type->data.array.elem_type);
		bassert(len && llvm_elem_type);
		llvm_values_t llvm_elems = SARR_ZERO;
		mir_instrs_t *values     = cmp->values;
		for (usize i = 0; i < sarrlenu(values); ++i) {
			struct mir_instr *it = sarrpeek(values, i);
			EMIT_NESTED_COMPOUND_IF_NEEDED(ctx, it);
			LLVMValueRef llvm_elem = it->llvm_value;
			bassert(LLVMIsConstant(llvm_elem) && "Expected constant!");
			sarrput(&llvm_elems, llvm_elem);
		}
		cmp->base.llvm_value = LLVMConstArray(llvm_elem_type, sarrdata(&llvm_elems), len);
		sarrfree(&llvm_elems);
		break;
	}

	case MIR_TYPE_STRING:
	case MIR_TYPE_DYNARR:
	case MIR_TYPE_SLICE:
	case MIR_TYPE_VARGS:
	case MIR_TYPE_STRUCT: {
		llvm_values_t  llvm_members = SARR_ZERO;
		mir_instrs_t  *values       = cmp->values;
		mir_members_t *members      = type->data.strct.members;
		bassert(members && values);
		const usize memc = sarrlenu(members);
		const usize valc = sarrlenu(values);
		sarraddn(&llvm_members, memc);
		memset(sarrdata(&llvm_members), 0, sizeof(LLVMValueRef) * sarrlenu(&llvm_members));

		usize index = 0;
		for (usize i = 0; i < valc; ++i, ++index) {
			if (mapping) index = sarrpeek(mapping, i);
			struct mir_instr *value = sarrpeek(values, i);
			bassert(value);

			EMIT_NESTED_COMPOUND_IF_NEEDED(ctx, value);
			sarrdata(&llvm_members)[index] = value->llvm_value;
		}

		for (usize i = 0; i < sarrlenu(&llvm_members); ++i) {
			if (sarrpeek(&llvm_members, i)) continue;
			struct mir_member *member  = sarrpeek(members, i);
			sarrdata(&llvm_members)[i] = LLVMConstNull(get_type(ctx, member->type));
		}

		cmp->base.llvm_value = LLVMConstNamedStruct(
		    get_type(ctx, type), sarrdata(&llvm_members), (u32)sarrlenu(&llvm_members));
		sarrfree(&llvm_members);
		break;
	}

	default:
		bassert(mapping == NULL && "Mapping not supported for fundamental types.");
		bassert(sarrlen(cmp->values) == 1 && "Expected only one compound initializer value!");
		struct mir_instr *it = sarrpeek(cmp->values, 0);
		EMIT_NESTED_COMPOUND_IF_NEEDED(ctx, it);
		LLVMValueRef llvm_value = it->llvm_value;
		bassert(LLVMIsConstant(llvm_value) && "Expected constant!");
		cmp->base.llvm_value = llvm_value;
	}

	bassert(cmp->base.llvm_value);
	return cmp->base.llvm_value;
}

void emit_instr_compound(struct ir_context         *ctx,
                         LLVMValueRef               llvm_dest,
                         struct mir_instr_compound *cmp) {
	bassert(llvm_dest && "Missing temp storage for compound value!");
	if (mir_is_zero_initialized(cmp)) {
		// Set tmp variable to zero when there are no values speficied.
		_emit_instr_compound_zero_initialized(ctx, llvm_dest, cmp);
		return;
	}
	if (mir_is_comptime(&cmp->base) && mir_is_global(&cmp->base)) {
		// Special case for global constants. (i.e. we cannot use memset intrinsic here).
		LLVMValueRef llvm_value = _emit_instr_compound_comptime(ctx, cmp);
		bassert(llvm_value);
		LLVMBuildStore(ctx->llvm_builder, llvm_value, llvm_dest);
		return;
	}

	// The rest is local, non-comptime and not zero initialized.
	struct mir_type *type = cmp->base.value.type;
	bassert(type);

	mir_instrs_t *values  = cmp->values;
	ints_t       *mapping = cmp->value_member_mapping;

	if (mir_is_composite_type(type) && sarrlenu(values) < sarrlenu(type->data.strct.members)) {
		// Zero initialization only for composit types when not all values are initialized.
		_emit_instr_compound_zero_initialized(ctx, llvm_dest, cmp);
	}

	LLVMValueRef llvm_value;
	LLVMValueRef llvm_indices[2];
	LLVMValueRef llvm_value_dest = llvm_dest;

	llvm_indices[0] = ctx->llvm_const_i64_zero;
	for (usize i = 0; i < sarrlenu(values); ++i) {
		struct mir_instr *value = sarrpeek(values, i);
		switch (type->kind) {
		case MIR_TYPE_ARRAY:
			bassert(mapping == NULL && "Mapping is not supported for arrays!");
			bassert(sarrlenu(values) == type->data.array.len);
			llvm_indices[1] = LLVMConstInt(get_type(ctx, ctx->builtin_types->t_s64), i, true);
			llvm_value_dest = LLVMBuildGEP2(ctx->llvm_builder,
			                                get_type(ctx, type),
			                                llvm_dest,
			                                llvm_indices,
			                                static_arrlenu(llvm_indices),
			                                "");
			break;

		case MIR_TYPE_DYNARR:
		case MIR_TYPE_STRING:
		case MIR_TYPE_SLICE:
		case MIR_TYPE_VARGS:
		case MIR_TYPE_STRUCT: {
			const usize index = mapping ? sarrpeek(mapping, i) : i;
			llvm_value_dest   = LLVMBuildStructGEP2(
                ctx->llvm_builder, get_type(ctx, type), llvm_dest, (unsigned int)index, "");
			break;
		}

		default:
			bassert(i == 0);
			bassert(mapping == NULL);
			bassert(sarrlenu(values) == 1);
			llvm_value_dest = llvm_dest;
			break;
		}

		if (value->kind == MIR_INSTR_COMPOUND) {
			struct mir_instr_compound *nested_cmp = (struct mir_instr_compound *)value;
			// @Incomplete: Why are we failing here?
			// bassert(!nested_cmp->is_naked && "Directly nested compounds cannot be naked!");
			emit_instr_compound(ctx, llvm_value_dest, nested_cmp);
		} else {
			llvm_value = value->llvm_value;
			bassert(llvm_value && "Missing LLVM value for nested compound expression member.");

			LLVMBuildStore(ctx->llvm_builder, llvm_value, llvm_value_dest);
		}
	}
}

enum ir_state emit_instr_binop(struct ir_context *ctx, struct mir_instr_binop *binop) {
	LLVMValueRef lhs = binop->lhs->llvm_value;
	LLVMValueRef rhs = binop->rhs->llvm_value;
	bassert(lhs && rhs);

	struct mir_type *type           = binop->lhs->value.type;
	const bool       real_type      = type->kind == MIR_TYPE_REAL;
	const bool       signed_integer = type->kind == MIR_TYPE_INT && type->data.integer.is_signed;
	DI_LOCATION_SET(&binop->base);
	switch (binop->op) {
	case BINOP_ADD:
		if (real_type)
			binop->base.llvm_value = LLVMBuildFAdd(ctx->llvm_builder, lhs, rhs, "");
		else
			binop->base.llvm_value = LLVMBuildAdd(ctx->llvm_builder, lhs, rhs, "");
		break;

	case BINOP_SUB:
		if (real_type)
			binop->base.llvm_value = LLVMBuildFSub(ctx->llvm_builder, lhs, rhs, "");
		else
			binop->base.llvm_value = LLVMBuildSub(ctx->llvm_builder, lhs, rhs, "");
		break;

	case BINOP_MUL:
		if (real_type)
			binop->base.llvm_value = LLVMBuildFMul(ctx->llvm_builder, lhs, rhs, "");
		else
			binop->base.llvm_value = LLVMBuildMul(ctx->llvm_builder, lhs, rhs, "");
		break;

	case BINOP_DIV:
		if (real_type)
			binop->base.llvm_value = LLVMBuildFDiv(ctx->llvm_builder, lhs, rhs, "");
		else if (signed_integer)
			binop->base.llvm_value = LLVMBuildSDiv(ctx->llvm_builder, lhs, rhs, "");
		else
			binop->base.llvm_value = LLVMBuildUDiv(ctx->llvm_builder, lhs, rhs, "");

		break;

	case BINOP_MOD:
		if (signed_integer)
			binop->base.llvm_value = LLVMBuildSRem(ctx->llvm_builder, lhs, rhs, "");
		else
			binop->base.llvm_value = LLVMBuildURem(ctx->llvm_builder, lhs, rhs, "");
		break;

	case BINOP_EQ:
		if (real_type)
			binop->base.llvm_value = LLVMBuildFCmp(ctx->llvm_builder, LLVMRealUEQ, lhs, rhs, "");
		else
			binop->base.llvm_value = LLVMBuildICmp(ctx->llvm_builder, LLVMIntEQ, lhs, rhs, "");
		break;

	case BINOP_NEQ:
		if (real_type)
			binop->base.llvm_value = LLVMBuildFCmp(ctx->llvm_builder, LLVMRealUNE, lhs, rhs, "");
		else
			binop->base.llvm_value = LLVMBuildICmp(ctx->llvm_builder, LLVMIntNE, lhs, rhs, "");
		break;

	case BINOP_GREATER:
		if (real_type)
			binop->base.llvm_value = LLVMBuildFCmp(ctx->llvm_builder, LLVMRealUGT, lhs, rhs, "");
		else
			binop->base.llvm_value = LLVMBuildICmp(
			    ctx->llvm_builder, signed_integer ? LLVMIntSGT : LLVMIntUGT, lhs, rhs, "");
		break;

	case BINOP_LESS:
		if (real_type)
			binop->base.llvm_value = LLVMBuildFCmp(ctx->llvm_builder, LLVMRealULT, lhs, rhs, "");
		else
			binop->base.llvm_value = LLVMBuildICmp(
			    ctx->llvm_builder, signed_integer ? LLVMIntSLT : LLVMIntULT, lhs, rhs, "");
		break;

	case BINOP_GREATER_EQ:
		if (real_type)
			binop->base.llvm_value = LLVMBuildFCmp(ctx->llvm_builder, LLVMRealUGE, lhs, rhs, "");
		else
			binop->base.llvm_value = LLVMBuildICmp(
			    ctx->llvm_builder, signed_integer ? LLVMIntSGE : LLVMIntUGE, lhs, rhs, "");
		break;

	case BINOP_LESS_EQ:
		if (real_type)
			binop->base.llvm_value = LLVMBuildFCmp(ctx->llvm_builder, LLVMRealULE, lhs, rhs, "");
		else
			binop->base.llvm_value = LLVMBuildICmp(
			    ctx->llvm_builder, signed_integer ? LLVMIntSLE : LLVMIntULE, lhs, rhs, "");
		break;

	case BINOP_AND:
		binop->base.llvm_value = LLVMBuildAnd(ctx->llvm_builder, lhs, rhs, "");
		break;

	case BINOP_OR:
		binop->base.llvm_value = LLVMBuildOr(ctx->llvm_builder, lhs, rhs, "");
		break;

	case BINOP_XOR:
		binop->base.llvm_value = LLVMBuildXor(ctx->llvm_builder, lhs, rhs, "");
		break;

	case BINOP_SHR:
		if (signed_integer)
			binop->base.llvm_value = LLVMBuildAShr(ctx->llvm_builder, lhs, rhs, "");
		else
			binop->base.llvm_value = LLVMBuildLShr(ctx->llvm_builder, lhs, rhs, "");
		break;

	case BINOP_SHL:
		binop->base.llvm_value = LLVMBuildShl(ctx->llvm_builder, lhs, rhs, "");
		break;

	default:
		babort("Invalid binary operation.");
	}
	DI_LOCATION_RESET();
	return STATE_PASSED;
}

// Create temporary variable for EASGM register split.
static inline LLVMValueRef
insert_easgm_tmp(struct ir_context *ctx, struct mir_instr_call *call, struct mir_type *type) {
	LLVMBasicBlockRef llvm_prev_block = LLVMGetInsertBlock(ctx->llvm_builder);
	LLVMBasicBlockRef llvm_entry_block =
	    LLVMValueAsBasicBlock(call->base.owner_block->owner_fn->first_block->base.llvm_value);
	if (LLVMGetLastInstruction(llvm_entry_block)) {
		LLVMPositionBuilderBefore(ctx->llvm_builder, LLVMGetLastInstruction(llvm_entry_block));
	} else {
		LLVMPositionBuilderAtEnd(ctx->llvm_builder, llvm_entry_block);
	}
	LLVMValueRef llvm_tmp = LLVMBuildAlloca(ctx->llvm_builder, get_type(ctx, type), "");
	LLVMPositionBuilderAtEnd(ctx->llvm_builder, llvm_prev_block);
	DI_LOCATION_RESET();
	return llvm_tmp;
}

enum ir_state emit_instr_call(struct ir_context *ctx, struct mir_instr_call *call) {
	bassert(!mir_is_comptime(&call->base) && "Compile time calls should not be generated into the final binary!");
	struct mir_instr *callee = call->callee;
	bassert(callee);
	bassert(callee->value.type);
	struct mir_type *callee_type = callee->value.type->kind == MIR_TYPE_FN ? callee->value.type : mir_deref_type(callee->value.type);
	bassert(callee_type);
	bassert(callee_type->kind == MIR_TYPE_FN);

	// There are three possible configurations:
	//
	// 1) We call regular static function; it's compile-time known and resolved by previous decl-ref instruction.
	// 2) We call via function pointer.
	// 3) We call immediate inline defined anonymous function (we have to generate one eventually).
	struct mir_fn *fn             = mir_is_comptime(callee) ? MIR_CEV_READ_AS(struct mir_fn *, &callee->value) : NULL;
	LLVMValueRef   llvm_called_fn = callee->llvm_value ? callee->llvm_value : emit_fn_proto(ctx, fn, true);

	bool       has_byval_arg = false;
	const bool has_args      = sarrlen(call->args) > 0;
	// Tmp for arg values passed into the Call Instruction.
	llvm_values_t llvm_args = SARR_ZERO;

	// Callee required argument types.
	LLVMTypeRef *llvm_callee_arg_types = NULL;
	LLVMValueRef llvm_result           = NULL;
	// SRET must come first!!!

	if (callee_type->data.fn.has_sret) {
		// PERFORMANCE: Reuse ret_tmp inside function???
		LLVMValueRef llvm_tmp = insert_easgm_tmp(ctx, call, callee_type->data.fn.ret_type);
		sarrput(&llvm_args, llvm_tmp);
	}

	LLVMTypeRef llvm_callee_type = get_type(ctx, callee_type);
	if (has_args) {
		// Get real argument types of LLMV function.
		arrsetlen(llvm_callee_arg_types, LLVMCountParamTypes(llvm_callee_type));
		LLVMGetParamTypes(llvm_callee_type, llvm_callee_arg_types);
		for (usize i = 0; i < sarrlenu(call->args); ++i) {
			struct mir_instr *arg_instr = sarrpeek(call->args, i);
			struct mir_arg   *arg       = sarrpeek(callee_type->data.fn.args, i);
			// Comptime arguments does not exist in LLVM.
			if (isflag(arg->flags, FLAG_COMPTIME)) continue;
			LLVMValueRef llvm_arg = arg_instr->llvm_value;

			switch (arg->llvm_easgm) {
			case LLVM_EASGM_NONE: { // Default behavior.
				sarrput(&llvm_args, llvm_arg);
				break;
			}

			case LLVM_EASGM_8:
			case LLVM_EASGM_16:
			case LLVM_EASGM_32:
			case LLVM_EASGM_64: { // Struct fits into one register.
				// Note: This is copy-paste from the next branch, see the description there...
				LLVMValueRef llvm_tmp = NULL;
				if (arg_instr->kind != MIR_INSTR_LOAD) {
					llvm_tmp = insert_easgm_tmp(ctx, call, arg->type);
					LLVMBuildStore(ctx->llvm_builder, llvm_arg, llvm_tmp);
				} else {
					// Load should load something already allocated, so no temporary is needed.
					bassert(arg_instr->kind == MIR_INSTR_LOAD);
					llvm_tmp = ((struct mir_instr_load *)arg_instr)->src->llvm_value;
				}
				bassert(llvm_tmp);

				llvm_tmp =
				    LLVMBuildBitCast(ctx->llvm_builder,
				                     llvm_tmp,
				                     LLVMPointerType(llvm_callee_arg_types[arg->llvm_index], 0),
				                     "");

				sarrput(
				    &llvm_args,
				    LLVMBuildLoad2(
				        ctx->llvm_builder, llvm_callee_arg_types[arg->llvm_index], llvm_tmp, ""));
				break;
			}

			case LLVM_EASGM_64_8:
			case LLVM_EASGM_64_16:
			case LLVM_EASGM_64_32:
			case LLVM_EASGM_64_64: { // Struct fits into two registers.
				// In the original solution we've used the temporary variable everytime, but this
				// produce overhead because we have to copy the data avery time; the simpliest
				// solution is to use prevous allocated variable directly, this might be checked by
				// arg_instr being load instr. Note that the load instruction is then unused, but
				// it's still better then copying every time...
				LLVMValueRef llvm_tmp = NULL;
				if (arg_instr->kind != MIR_INSTR_LOAD) {
					llvm_tmp = insert_easgm_tmp(ctx, call, arg->type);
					LLVMBuildStore(ctx->llvm_builder, llvm_arg, llvm_tmp);
				} else {
					// Load should load something already allocated, so no temporary is needed.
					bassert(arg_instr->kind == MIR_INSTR_LOAD);
					llvm_tmp = ((struct mir_instr_load *)arg_instr)->src->llvm_value;
				}
				bassert(llvm_tmp);

				// Create dummy type just to get propper offsets for GEPs.
				LLVMTypeRef llvm_tmp_elem_types[] = {
				    llvm_callee_arg_types[arg->llvm_index],
				    llvm_callee_arg_types[arg->llvm_index + 1],
				};
				LLVMTypeRef llvm_tmp_type = llvm_struct_type_in_context(ctx->llvm_cnt, llvm_tmp_elem_types, 2, false);

				LLVMValueRef llvm_tmp_1 =
				    LLVMBuildStructGEP2(ctx->llvm_builder, llvm_tmp_type, llvm_tmp, 0, "");
				sarrput(&llvm_args,
				        LLVMBuildLoad2(ctx->llvm_builder, llvm_tmp_elem_types[0], llvm_tmp_1, ""));

				LLVMValueRef llvm_tmp_2 =
				    LLVMBuildStructGEP2(ctx->llvm_builder, llvm_tmp_type, llvm_tmp, 1, "");
				sarrput(&llvm_args,
				        LLVMBuildLoad2(ctx->llvm_builder, llvm_tmp_elem_types[1], llvm_tmp_2, ""));
				break;
			}

			case LLVM_EASGM_BYVAL: { // Struct is too big and must be passed by value.
				if (!has_byval_arg) has_byval_arg = true;
				LLVMValueRef llvm_tmp = NULL;
				if (arg_instr->kind == MIR_INSTR_COMPOUND) {
					llvm_tmp =
					    ((struct mir_instr_compound *)arg_instr)->tmp_var->llvm_value;
				} else if (arg_instr->kind != MIR_INSTR_LOAD) {
					llvm_tmp = insert_easgm_tmp(ctx, call, arg->type);
					LLVMBuildStore(ctx->llvm_builder, llvm_arg, llvm_tmp);
				} else {
					bassert(arg_instr->kind == MIR_INSTR_LOAD);
					llvm_tmp = ((struct mir_instr_load *)arg_instr)->src->llvm_value;
				}
				bassert(llvm_tmp);
				sarrput(&llvm_args, llvm_tmp);
				break;
			}
			}
		}
	}
	DI_LOCATION_SET(&call->base);
	LLVMValueRef llvm_call = LLVMBuildCall2(ctx->llvm_builder,
	                                        llvm_callee_type,
	                                        llvm_called_fn,
	                                        sarrdata(&llvm_args),
	                                        (u32)sarrlenu(&llvm_args),
	                                        "");
	DI_LOCATION_RESET();

	if (callee_type->data.fn.has_sret) {
		struct mir_type *sret_arg_type = callee_type->data.fn.ret_type;
		LLVMAddCallSiteAttribute(llvm_call,
		                         LLVM_SRET_INDEX + 1,
		                         llvm_create_type_attribute(ctx->llvm_cnt,
		                                                    LLVM_ATTR_STRUCTRET,
		                                                    get_type(ctx, sret_arg_type)));

		LLVMAddCallSiteAttribute(llvm_call,
		                         LLVM_SRET_INDEX + 1,
		                         llvm_create_enum_attribute(ctx->llvm_cnt,
		                                                    LLVM_ATTR_ALIGNMENT,
		                                                    (uint64_t)sret_arg_type->alignment));

		llvm_result = LLVMBuildLoad2(ctx->llvm_builder,
		                             get_type(ctx, callee_type->data.fn.ret_type),
		                             sarrpeek(&llvm_args, LLVM_SRET_INDEX),
		                             "");
	}

	// @Performance: LLVM API requires to set call side attributes after call is created.
	// @Incomplete: Disabled for now, this for some reason does not work on ARM. Check if it's
	// needed on other platforms.
	if (has_byval_arg) {
		bassert(has_args);
		mir_args_t *args = callee_type->data.fn.args;
		for (usize i = 0; i < sarrlenu(args); ++i) {
			struct mir_arg *arg = sarrpeek(args, i);
			if (arg->llvm_easgm != LLVM_EASGM_BYVAL) continue;
			LLVMTypeRef llvm_arg_type = get_type(ctx, arg->type);
			LLVMAddCallSiteAttribute(
			    llvm_call,
			    arg->llvm_index + 1,
			    llvm_create_type_attribute(ctx->llvm_cnt, LLVM_ATTR_BYVAL, llvm_arg_type));
		}
	}
	arrfree(llvm_callee_arg_types);
	sarrfree(&llvm_args);
	call->base.llvm_value = llvm_result ? llvm_result : llvm_call;
	return STATE_PASSED;
}

enum ir_state emit_instr_set_initializer(struct ir_context *ctx, struct mir_instr_set_initializer *si) {
	struct mir_instr *dest = si->dest;
	struct mir_var   *var  = ((struct mir_instr_decl_var *)dest)->var;
	if (var->ref_count > 0) {
		bassert(var->llvm_value);
		LLVMValueRef llvm_init_value = si->src->llvm_value;
		if (!llvm_init_value) {
			bassert(si->src->kind == MIR_INSTR_COMPOUND);
			llvm_init_value = emit_instr_compound_global(ctx, (struct mir_instr_compound *)si->src);
		}
		bassert(llvm_init_value);
		LLVMSetInitializer(var->llvm_value, llvm_init_value);
	}
	return STATE_PASSED;
}

enum ir_state emit_instr_decl_var(struct ir_context *ctx, struct mir_instr_decl_var *decl) {
	struct mir_var *var = decl->var;
	bassert(var);
	if (var->ref_count == 0) return STATE_PASSED;

	// Implicit variables are not supposed to have debug info, but even non-implicit ones could have
	// missing decl_node, implicit in this case means it's just not explicitly inserted into scope,
	// but non-implicit variable can be i.e. variable generated by compiler (IS_DEBUG) which has no
	// user definition in code. @CLEANUP This is little bit confusing, we should unify meaning of
	// `implicit` across the compiler.
	const bool emit_DI =
	    ctx->generate_debug_info && isnotflag(var->iflags, MIR_VAR_IMPLICIT) && var->decl_node;

	// Skip when we should not generate LLVM representation
	if (!mir_type_has_llvm_representation(var->value.type)) return STATE_PASSED;
	// Since we introduced lazy generation of IR instructions (mainly to reduce size of generated
	// binary and also to speed up compilation times) we do not generated global variables as they
	// come in global scope, but only in case they are used. (see also emit_global_var_proto).
	bassert(isnotflag(var->iflags, MIR_VAR_GLOBAL) &&
	        "Global variable IR is supposed to lazy generated only as needed!");
	bassert(var->llvm_value);
	if (decl->init) {
		if (decl->init->kind == MIR_INSTR_COMPOUND) {
			// There is special handling for initialization via compound instruction
			emit_instr_compound(ctx, var->llvm_value, (struct mir_instr_compound *)decl->init);
		} else if (decl->init->kind == MIR_INSTR_ARG) {
			// In case the comptime function argument is not converted to constant (we convert only
			// fundamental type) we have to simply duplicate the value of initializer here.
			emit_instr_arg(ctx, var, (struct mir_instr_arg *)decl->init);
		} else {
			// use simple store
			LLVMValueRef llvm_init = decl->init->llvm_value;
			bassert(llvm_init);
			const usize size_bytes = var->value.type->store_size_bytes;
			if (false && size_bytes > STORE_MAX_SIZE_BYTES &&
			    isnotflag(var->iflags, MIR_VAR_GLOBAL)) {
				build_call_memcpy(ctx, llvm_init, var->llvm_value, size_bytes);
			} else {
				LLVMBuildStore(ctx->llvm_builder, llvm_init, var->llvm_value);
			}
		}
	}
	if (emit_DI) emit_DI_var(ctx, var);
	return STATE_PASSED;
}

enum ir_state emit_instr_ret(struct ir_context *ctx, struct mir_instr_ret *ret) {
	struct mir_fn *fn = ret->base.owner_block->owner_fn;
	bassert(fn);

	struct mir_type *fn_type = fn->type;
	bassert(fn_type);
	DI_LOCATION_SET(&ret->base);

	if (fn_type->data.fn.has_sret) {
		LLVMValueRef llvm_ret_value = ret->value->llvm_value;
		LLVMValueRef llvm_sret      = LLVMGetParam(fn->llvm_value, LLVM_SRET_INDEX);
		LLVMBuildStore(ctx->llvm_builder, llvm_ret_value, llvm_sret);
		ret->base.llvm_value = LLVMBuildRetVoid(ctx->llvm_builder);
		goto FINALIZE;
	}

	if (ret->value) {
		LLVMValueRef llvm_ret_value = ret->value->llvm_value;
		bassert(llvm_ret_value);
		ret->base.llvm_value = LLVMBuildRet(ctx->llvm_builder, llvm_ret_value);
		goto FINALIZE;
	}

	ret->base.llvm_value = LLVMBuildRetVoid(ctx->llvm_builder);

FINALIZE:
	DI_LOCATION_RESET();
	return STATE_PASSED;
}

enum ir_state emit_instr_br(struct ir_context *ctx, struct mir_instr_br *br) {
	struct mir_instr_block *then_block = br->then_block;
	bassert(then_block);
	LLVMBasicBlockRef llvm_then_block = emit_basic_block(ctx, then_block);
	bassert(llvm_then_block);
	DI_LOCATION_SET(&br->base);
	br->base.llvm_value = LLVMBuildBr(ctx->llvm_builder, llvm_then_block);
	DI_LOCATION_RESET();
	LLVMPositionBuilderAtEnd(ctx->llvm_builder, llvm_then_block);
	return STATE_PASSED;
}

enum ir_state emit_instr_switch(struct ir_context *ctx, struct mir_instr_switch *sw) {
	struct mir_instr       *value              = sw->value;
	struct mir_instr_block *default_block      = sw->default_block;
	mir_switch_cases_t     *cases              = sw->cases;
	LLVMValueRef            llvm_value         = value->llvm_value;
	LLVMBasicBlockRef       llvm_default_block = emit_basic_block(ctx, default_block);
	LLVMValueRef            llvm_switch =
	    LLVMBuildSwitch(ctx->llvm_builder, llvm_value, llvm_default_block, (u32)sarrlenu(cases));
	for (usize i = 0; i < sarrlenu(cases); ++i) {
		struct mir_switch_case *c             = &sarrpeek(cases, i);
		LLVMValueRef            llvm_on_value = c->on_value->llvm_value;
		LLVMBasicBlockRef       llvm_block    = emit_basic_block(ctx, c->block);
		LLVMAddCase(llvm_switch, llvm_on_value, llvm_block);
	}
	sw->base.llvm_value = llvm_switch;
	return STATE_PASSED;
}

enum ir_state emit_instr_const(struct ir_context *ctx, struct mir_instr_const *c) {
	struct mir_type *type = c->base.value.type;
	if (type->kind == MIR_TYPE_NAMED_SCOPE) {
		// Named scope is just mir related temporary type, it has no LLVM representation!
		return STATE_PASSED;
	}
	LLVMValueRef llvm_value = NULL;
	LLVMTypeRef  llvm_type  = get_type(ctx, type);
	switch (type->kind) {
	case MIR_TYPE_ENUM: {
		type = type->data.enm.base_type;
		bassert(type->kind == MIR_TYPE_INT);
		const u64 i = vm_read_int(type, c->base.value.data);
		llvm_value  = LLVMConstInt(llvm_type, i, type->data.integer.is_signed);
		break;
	}
	case MIR_TYPE_INT: {
		const u64 i = vm_read_int(type, c->base.value.data);
		llvm_value  = LLVMConstInt(llvm_type, i, type->data.integer.is_signed);
		break;
	}
	case MIR_TYPE_BOOL: {
		const bool i = (bool)vm_read_int(type, c->base.value.data);
		llvm_value   = LLVMConstInt(llvm_type, i, false);
		break;
	}
	case MIR_TYPE_REAL: {
		switch (type->store_size_bytes) {
		case 4: {
			const float i = vm_read_float(type, c->base.value.data);
			llvm_value    = LLVMConstReal(llvm_type, (double)i);
			break;
		}
		case 8: {
			const double i = vm_read_double(type, c->base.value.data);
			llvm_value     = LLVMConstReal(llvm_type, i);
			break;
		}
		default:
			babort("Unknown real type!");
		}
		break;
	}
	case MIR_TYPE_PTR:
		// Currently used only for default intialization of pointer values, so we expect this to be NULL!
		bassert(vm_read_ptr(type, c->base.value.data) == NULL);
		// fall-through
	case MIR_TYPE_NULL: {
		llvm_value = LLVMConstNull(llvm_type);
		break;
	}
	case MIR_TYPE_SLICE: {
		if (type->data.strct.is_string_literal) {
			vm_stack_ptr_t len_ptr = vm_get_struct_elem_ptr(ctx->assembly, type, c->base.value.data, 0);
			vm_stack_ptr_t str_ptr = vm_get_struct_elem_ptr(ctx->assembly, type, c->base.value.data, 1);
			const s64      len     = vm_read_as(s64, len_ptr);
			char          *str     = vm_read_as(char *, str_ptr);
			llvm_value             = emit_const_string(ctx, make_str(str, len));
		} else {
			// Only string literals can be represented as constant for now! Other slices are not
			// handled.
			BL_UNIMPLEMENTED;
		}
		break;
	}
	case MIR_TYPE_FN: {
		struct mir_fn *fn = (struct mir_fn *)vm_read_ptr(type, c->base.value.data);
		bmagic_assert(fn);
		llvm_value = emit_fn_proto(ctx, fn, true);
		break;
	}
	case MIR_TYPE_VOID: {
		return STATE_PASSED;
	}
	case MIR_TYPE_STRUCT: {
		// More complex data blobs can be produced by comptime call evaluation, we split struct data
		// into individual values wrapped into const instruction to reuse the same function here.
		llvm_values_t  llvm_members = SARR_ZERO;
		mir_members_t *members      = type->data.strct.members;
		for (usize i = 0; i < members->len; ++i) {
			struct mir_member *member = sarrpeek(members, i);
			vm_stack_ptr_t     value_ptr =
			    vm_get_struct_elem_ptr(ctx->assembly, type, c->base.value.data, (u32)i);

			struct mir_instr_const tmp = {
			    .base.value.type        = member->type, // get_type is called internally
			    .base.value.data        = value_ptr,
			    .base.value.is_comptime = true,
			};

			if (emit_instr_const(ctx, &tmp) != STATE_PASSED) {
				babort("Cannot evaluate constant data blob!");
			}
			bassert(tmp.base.llvm_value);
			sarrput(&llvm_members, tmp.base.llvm_value);
		}
		llvm_value =
		    LLVMConstNamedStruct(llvm_type, sarrdata(&llvm_members), sarrlen(&llvm_members));
		sarrfree(&llvm_members);
		break;
	}
	case MIR_TYPE_ARRAY: {
		llvm_values_t    llvm_elems = SARR_ZERO;
		struct mir_type *elem_type  = type->data.array.elem_type;
		const s64        elemc      = type->data.array.len;
		for (s64 i = 0; i < elemc; ++i) {
			vm_stack_ptr_t         value_ptr = vm_get_array_elem_ptr(type, c->base.value.data, (u32)i);
			struct mir_instr_const tmp       = {
			          .base.value.type        = elem_type,
			          .base.value.data        = value_ptr,
			          .base.value.is_comptime = true,
            };
			if (emit_instr_const(ctx, &tmp) != STATE_PASSED) {
				babort("Cannot evaluate constant data blob!");
			}
			bassert(tmp.base.llvm_value);
			sarrput(&llvm_elems, tmp.base.llvm_value);
		}
		llvm_value =
		    LLVMConstArray(elem_type->llvm_type, sarrdata(&llvm_elems), sarrlen(&llvm_elems));
		sarrfree(&llvm_elems);
		break;
	}
	default:
		BL_UNIMPLEMENTED;
	}
	bassert(llvm_value && "Incomplete const value generation!");
	c->base.llvm_value = llvm_value;
	return STATE_PASSED;
}

enum ir_state emit_instr_cond_br(struct ir_context *ctx, struct mir_instr_cond_br *br) {
	struct mir_instr       *cond       = br->cond;
	struct mir_instr_block *then_block = br->then_block;
	struct mir_instr_block *else_block = br->else_block;
	bassert(cond && then_block);
	LLVMValueRef      llvm_cond       = cond->llvm_value;
	LLVMBasicBlockRef llvm_then_block = emit_basic_block(ctx, then_block);
	LLVMBasicBlockRef llvm_else_block = emit_basic_block(ctx, else_block);
	DI_LOCATION_SET(&br->base);
	br->base.llvm_value =
	    LLVMBuildCondBr(ctx->llvm_builder, llvm_cond, llvm_then_block, llvm_else_block);
	DI_LOCATION_RESET();
	return STATE_PASSED;
}

enum ir_state emit_instr_vargs(struct ir_context *ctx, struct mir_instr_vargs *vargs) {
	struct mir_type *vargs_type = vargs->base.value.type;
	mir_instrs_t    *values     = vargs->values;
	bassert(values);
	const usize vargsc = sarrlenu(values);
	bassert(vargs_type && vargs_type->kind == MIR_TYPE_VARGS);
	struct mir_type *vargs_tmp_type = vargs->vargs_tmp->value.type;

	// Setup tmp array values.
	if (vargsc > 0) {
		LLVMValueRef llvm_indices[2];
		llvm_indices[0]                 = ctx->llvm_const_i64_zero;
		struct mir_type *vargs_arr_type = vargs->arr_tmp->value.type;
		bassert(vargs_arr_type);

		for (usize i = 0; i < vargsc; ++i) {
			struct mir_instr *value      = sarrpeek(values, i);
			LLVMValueRef      llvm_value = value->llvm_value;
			bassert(llvm_value);
			llvm_indices[1]              = LLVMConstInt(get_type(ctx, ctx->builtin_types->t_s64), i, true);
			LLVMValueRef llvm_value_dest = LLVMBuildGEP2(ctx->llvm_builder,
			                                             get_type(ctx, vargs_arr_type),
			                                             vargs->arr_tmp->llvm_value,
			                                             llvm_indices,
			                                             static_arrlenu(llvm_indices),
			                                             "");
			LLVMBuildStore(ctx->llvm_builder, llvm_value, llvm_value_dest);
		}
	}

	{
		LLVMValueRef llvm_len =
		    LLVMConstInt(get_type(ctx, ctx->builtin_types->t_s64), vargsc, false);

		LLVMValueRef llvm_dest = LLVMBuildStructGEP2(ctx->llvm_builder,
		                                             get_type(ctx, vargs_tmp_type),
		                                             vargs->vargs_tmp->llvm_value,
		                                             MIR_SLICE_LEN_INDEX,
		                                             "");
		LLVMBuildStore(ctx->llvm_builder, llvm_len, llvm_dest);

		LLVMTypeRef  llvm_ptr_type = get_type(ctx, mir_get_struct_elem_type(vargs_type, 1));
		LLVMValueRef llvm_ptr =
		    vargs->arr_tmp ? vargs->arr_tmp->llvm_value : LLVMConstNull(llvm_ptr_type);
		llvm_dest = LLVMBuildStructGEP2(ctx->llvm_builder,
		                                get_type(ctx, vargs_tmp_type),
		                                vargs->vargs_tmp->llvm_value,
		                                MIR_SLICE_PTR_INDEX,
		                                "");
		LLVMBuildStore(ctx->llvm_builder, llvm_ptr, llvm_dest);
	}

	vargs->base.llvm_value = LLVMBuildLoad2(ctx->llvm_builder,
	                                        get_type(ctx, vargs->vargs_tmp->value.type),
	                                        vargs->vargs_tmp->llvm_value,
	                                        "");
	return STATE_PASSED;
}

enum ir_state emit_instr_toany(struct ir_context *ctx, struct mir_instr_to_any *toany) {
	LLVMValueRef llvm_dest      = toany->tmp->llvm_value;
	LLVMValueRef llvm_type_info = rtti_emit(ctx, toany->rtti_type);

	bassert(llvm_dest && llvm_type_info);
	// Setup tmp variable pointer to type info
	struct mir_type *tmp_type            = toany->tmp->value.type;
	LLVMValueRef     llvm_dest_type_info = LLVMBuildStructGEP2(ctx->llvm_builder, get_type(ctx, tmp_type), llvm_dest, 0, "");
	LLVMBuildStore(ctx->llvm_builder, llvm_type_info, llvm_dest_type_info);

	// data
	struct mir_type *data_type           = mir_get_struct_elem_type(toany->tmp->value.type, 1);
	LLVMTypeRef      llvm_dest_data_type = get_type(ctx, data_type);
	LLVMValueRef     llvm_dest_data      = LLVMBuildStructGEP2(ctx->llvm_builder, get_type(ctx, tmp_type), llvm_dest, 1, "");

	if (toany->expr_tmp) {
		LLVMValueRef llvm_dest_tmp = toany->expr_tmp->llvm_value;
		bassert(llvm_dest_tmp && "Missing tmp variable!");
		LLVMBuildStore(ctx->llvm_builder, toany->expr->llvm_value, llvm_dest_tmp);
		LLVMValueRef llvm_data = LLVMBuildPointerCast(ctx->llvm_builder, llvm_dest_tmp, llvm_dest_data_type, "");
		LLVMBuildStore(ctx->llvm_builder, llvm_data, llvm_dest_data);
	} else if (toany->rtti_data) {
		LLVMValueRef llvm_data_type_info = rtti_emit(ctx, toany->rtti_data);
		LLVMValueRef llvm_data           = LLVMBuildPointerCast(ctx->llvm_builder, llvm_data_type_info, llvm_dest_data_type, "");
		LLVMBuildStore(ctx->llvm_builder, llvm_data, llvm_dest_data);
	} else {
		LLVMValueRef llvm_data = LLVMBuildPointerCast(ctx->llvm_builder, toany->expr->llvm_value, llvm_dest_data_type, ""); // @Cleanup: We might don't need this cast.
		LLVMBuildStore(ctx->llvm_builder, llvm_data, llvm_dest_data);
	}

	toany->base.llvm_value = llvm_dest;
	return STATE_PASSED;
}

enum ir_state emit_instr_call_loc(struct ir_context *ctx, struct mir_instr_call_loc *loc) {
	struct mir_var *meta_var = loc->meta_var;
	bassert(meta_var);
	struct mir_type *type = meta_var->value.type;
	LLVMValueRef     llvm_var =
	    llvm_add_global(ctx->llvm_module, get_type(ctx, type), meta_var->linkage_name);
	LLVMSetLinkage(llvm_var, LLVMPrivateLinkage);
	LLVMSetGlobalConstant(llvm_var, true);

	LLVMValueRef llvm_vals[4];
	const str_t  filepath      = loc->call_location->unit->filepath;
	llvm_vals[0]               = emit_const_string(ctx, filepath);
	struct mir_type *line_type = mir_get_struct_elem_type(type, 1);
	llvm_vals[1]               = LLVMConstInt(get_type(ctx, line_type), (u32)loc->call_location->line, true);
	llvm_vals[2]               = emit_const_string(ctx, loc->function_name);
	struct mir_type *hash_type = mir_get_struct_elem_type(type, 3);
	llvm_vals[3]               = LLVMConstInt(get_type(ctx, hash_type), (u32)loc->hash, false);
	LLVMValueRef llvm_value =
	    LLVMConstNamedStruct(get_type(ctx, type), llvm_vals, static_arrlenu(llvm_vals));

	LLVMSetInitializer(llvm_var, llvm_value);
	loc->meta_var->llvm_value = llvm_var;
	loc->base.llvm_value      = llvm_var;
	return STATE_PASSED;
}

enum ir_state emit_instr_block(struct ir_context *ctx, struct mir_instr_block *block) {
	// We don't want to generate type resolvers for typedefs!!!
	struct mir_fn    *fn              = block->owner_fn;
	const bool        is_global       = fn == NULL;
	LLVMBasicBlockRef llvm_prev_block = LLVMGetInsertBlock(ctx->llvm_builder);

	if (!block->terminal) babort("Block '%s', is not terminated", block->name);

	// Global-scope blocks does not have LLVM equivalent, we can generate just the
	// content of our block, but every instruction must be comptime constant.
	if (!is_global) {
		bassert(fn->llvm_value);
		LLVMBasicBlockRef llvm_block = emit_basic_block(ctx, block);
		bassert(llvm_block);

		LLVMPositionBuilderAtEnd(ctx->llvm_builder, llvm_block);

		// gen allocas fist in entry block!!!
		if (fn->first_block == block) {
			emit_allocas(ctx, fn);
		}
	} else {
		bassert(block->base.value.is_comptime);
	}

	// Generate all instructions in the block.
	struct mir_instr *instr = block->entry_instr;
	while (instr) {
		const enum ir_state s = emit_instr(ctx, instr);
		if (s == STATE_POSTPONE) {
			qpush_back(&ctx->incomplete_queue, instr);
			goto SKIP;
		}
		instr = instr->next;
	}

SKIP:
	LLVMPositionBuilderAtEnd(ctx->llvm_builder, llvm_prev_block);
	return STATE_PASSED;
}

void emit_incomplete(struct ir_context *ctx) {
	LLVMBasicBlockRef prev_bb = LLVMGetInsertBlock(ctx->llvm_builder);
	while (qmaybeswap(&ctx->incomplete_queue)) {
		struct mir_instr *instr = qpop_front(&ctx->incomplete_queue);
		while (instr) {
			struct mir_instr_block *bb = instr->owner_block;
			if (!mir_is_global_block(bb)) {
				LLVMBasicBlockRef llvm_bb = LLVMValueAsBasicBlock(bb->base.llvm_value);
				LLVMPositionBuilderAtEnd(ctx->llvm_builder, llvm_bb);
			}
			if (emit_instr(ctx, instr) == STATE_POSTPONE) {
				qpush_back(&ctx->incomplete_queue, instr);
				break;
			}
			instr = instr->next;
		}
	}
	LLVMPositionBuilderAtEnd(ctx->llvm_builder, prev_bb);
}

void emit_allocas(struct ir_context *ctx, struct mir_fn *fn) {
	bassert(fn);
	str_t       var_name;
	LLVMTypeRef var_type;
	unsigned    var_alignment;

	for (usize i = 0; i < arrlenu(fn->variables); ++i) {
		struct mir_var *var = fn->variables[i];
		bassert(var);
		if (isnotflag(var->iflags, MIR_VAR_EMIT_LLVM)) continue;
		if (var->ref_count == 0) continue;
#if NAMED_VARS
		var_name = var->linkage_name;
#else
		var_name = str_empty;
#endif
		var_type      = get_type(ctx, var->value.type);
		var_alignment = (unsigned int)var->value.type->alignment;
		bassert(var_type);
		var->llvm_value = llvm_build_alloca(ctx->llvm_builder, var_type, var_name);
		LLVMSetAlignment(var->llvm_value, var_alignment);
	}
}

enum ir_state emit_instr_fn_proto(struct ir_context *ctx, struct mir_instr_fn_proto *fn_proto) {
	struct mir_fn *fn = MIR_CEV_READ_AS(struct mir_fn *, &fn_proto->base.value);
	bmagic_assert(fn);
	emit_fn_proto(ctx, fn, false);

	// External functions does not have any body block.
	if (isnotflag(fn->flags, FLAG_EXTERN) && isnotflag(fn->flags, FLAG_INTRINSIC)) {
		if (ctx->generate_debug_info) emit_DI_fn(ctx, fn);
		// Generate all blocks in the function body.
		struct mir_instr_block *block = fn->first_block;
		while (block) {
			bassert(block->base.kind == MIR_INSTR_BLOCK);
			if (!block->is_unreachable && (block->base.ref_count == MIR_NO_REF_COUNTING || block->base.ref_count > 0)) {
				const enum ir_state s = emit_instr(ctx, &block->base);
				if (s != STATE_PASSED) babort("Postpone for whole block is not supported!");
			}
			block = (struct mir_instr_block *)block->base.next;
		}
	}

	return STATE_PASSED;
}

enum ir_state emit_instr(struct ir_context *ctx, struct mir_instr *instr) {
	enum ir_state state = STATE_PASSED;
	bassert(instr->state == MIR_IS_COMPLETE && "Attempt to emit instruction in incomplete state!");
	if (!mir_type_has_llvm_representation((instr->value.type))) return state;
	switch (instr->kind) {
	case MIR_INSTR_INVALID:
		babort("Invalid instruction");

	case MIR_INSTR_SIZEOF:
	case MIR_INSTR_ALIGNOF:
	case MIR_INSTR_DECL_VARIANT:
	case MIR_INSTR_DECL_MEMBER:
	case MIR_INSTR_DECL_ARG:
	case MIR_INSTR_TYPE_FN:
	case MIR_INSTR_TYPE_STRUCT:
	case MIR_INSTR_TYPE_PTR:
	case MIR_INSTR_TYPE_ARRAY:
	case MIR_INSTR_TYPE_SLICE:
	case MIR_INSTR_TYPE_DYNARR:
	case MIR_INSTR_TYPE_VARGS:
	case MIR_INSTR_TYPE_ENUM:
	case MIR_INSTR_ARG:
		break;

	case MIR_INSTR_BINOP:
		state = emit_instr_binop(ctx, (struct mir_instr_binop *)instr);
		break;
	case MIR_INSTR_FN_PROTO:
		state = emit_instr_fn_proto(ctx, (struct mir_instr_fn_proto *)instr);
		break;
	case MIR_INSTR_BLOCK:
		state = emit_instr_block(ctx, (struct mir_instr_block *)instr);
		break;
	case MIR_INSTR_BR:
		state = emit_instr_br(ctx, (struct mir_instr_br *)instr);
		break;
	case MIR_INSTR_SWITCH:
		state = emit_instr_switch(ctx, (struct mir_instr_switch *)instr);
		break;
	case MIR_INSTR_COND_BR:
		state = emit_instr_cond_br(ctx, (struct mir_instr_cond_br *)instr);
		break;
	case MIR_INSTR_RET:
		state = emit_instr_ret(ctx, (struct mir_instr_ret *)instr);
		break;
	case MIR_INSTR_DECL_VAR:
		state = emit_instr_decl_var(ctx, (struct mir_instr_decl_var *)instr);
		break;
	case MIR_INSTR_DECL_REF:
		state = emit_instr_decl_ref(ctx, (struct mir_instr_decl_ref *)instr);
		break;
	case MIR_INSTR_LOAD:
		state = emit_instr_load(ctx, (struct mir_instr_load *)instr);
		break;
	case MIR_INSTR_STORE:
		state = emit_instr_store(ctx, (struct mir_instr_store *)instr);
		break;
	case MIR_INSTR_CALL:
		state = emit_instr_call(ctx, (struct mir_instr_call *)instr);
		break;
	case MIR_INSTR_UNOP:
		state = emit_instr_unop(ctx, (struct mir_instr_unop *)instr);
		break;
	case MIR_INSTR_UNREACHABLE:
		state = emit_instr_unreachable(ctx, (struct mir_instr_unreachable *)instr);
		break;
	case MIR_INSTR_DEBUGBREAK:
		state = emit_instr_debugbreak(ctx, (struct mir_instr_debugbreak *)instr);
		break;
	case MIR_INSTR_MEMBER_PTR:
		state = emit_instr_member_ptr(ctx, (struct mir_instr_member_ptr *)instr);
		break;
	case MIR_INSTR_ELEM_PTR:
		state = emit_instr_elem_ptr(ctx, (struct mir_instr_elem_ptr *)instr);
		break;
	case MIR_INSTR_ADDROF:
		state = emit_instr_addrof(ctx, (struct mir_instr_addrof *)instr);
		break;
	case MIR_INSTR_CAST:
		state = emit_instr_cast(ctx, (struct mir_instr_cast *)instr);
		break;
	case MIR_INSTR_VARGS:
		state = emit_instr_vargs(ctx, (struct mir_instr_vargs *)instr);
		break;
	case MIR_INSTR_TYPE_INFO:
		state = emit_instr_type_info(ctx, (struct mir_instr_type_info *)instr);
		break;
	case MIR_INSTR_PHI:
		state = emit_instr_phi(ctx, (struct mir_instr_phi *)instr);
		break;
	case MIR_INSTR_COMPOUND: {
		struct mir_instr_compound *cmp = (struct mir_instr_compound *)instr;
		if (!cmp->is_naked) break;
		state = emit_instr_compound_naked(ctx, cmp);
		break;
	}
	case MIR_INSTR_TOANY:
		state = emit_instr_toany(ctx, (struct mir_instr_to_any *)instr);
		break;
	case MIR_INSTR_DECL_DIRECT_REF:
		state = emit_instr_decl_direct_ref(ctx, (struct mir_instr_decl_direct_ref *)instr);
		break;
	case MIR_INSTR_CONST:
		state = emit_instr_const(ctx, (struct mir_instr_const *)instr);
		break;
	case MIR_INSTR_SET_INITIALIZER:
		state = emit_instr_set_initializer(ctx, (struct mir_instr_set_initializer *)instr);
		break;
	case MIR_INSTR_TEST_CASES:
		state = emit_instr_test_cases(ctx, (struct mir_instr_test_case *)instr);
		break;
	case MIR_INSTR_CALL_LOC:
		state = emit_instr_call_loc(ctx, (struct mir_instr_call_loc *)instr);
		break;
	case MIR_INSTR_UNROLL:
		state = emit_instr_unroll(ctx, (struct mir_instr_unroll *)instr);
		break;
	default:
		babort("Missing emit instruction!");
	}
	if (state == STATE_PASSED) ctx->emit_instruction_count++;
	return state;
}

static void init_llvm_module(struct ir_context *ctx) {
	struct assembly *assembly    = ctx->assembly;
	LLVMModuleRef    llvm_module = llvm_module_create_with_name_in_context(ctx->llvm_cnt, assembly->target->name);
	LLVMSetTarget(llvm_module, assembly->llvm.triple);
	LLVMSetModuleDataLayout(llvm_module, assembly->llvm.TD);
	ctx->llvm_module = assembly->llvm.module = llvm_module;
}

static void intrinsics_init(struct ir_context *ctx) {
	// lookup intrinsics
	{ // memset
		LLVMTypeRef pt[2];

		pt[0] = get_type(ctx, ctx->builtin_types->t_u8_ptr);
		pt[1] = get_type(ctx, ctx->builtin_types->t_u64);

		const unsigned id = LLVM_MEMSET_INTRINSIC_ID;

		ctx->intrinsic_memset =
		    LLVMGetIntrinsicDeclaration(ctx->llvm_module, id, pt, static_arrlenu(pt));
		ctx->intrinsic_memset_type = llvm_intrinsic_get_type(ctx->llvm_cnt, id, pt, static_arrlenu(pt));

		bassert(ctx->intrinsic_memset && "Invalid memset intrinsic!");
	}

	{ // memcpy
		LLVMTypeRef pt[3];
		pt[0] = get_type(ctx, ctx->builtin_types->t_u8_ptr);
		pt[1] = get_type(ctx, ctx->builtin_types->t_u8_ptr);
		pt[2] = get_type(ctx, ctx->builtin_types->t_u64);

		const unsigned id = LLVM_MEMCPY_INTRINSIC_ID;

		ctx->intrinsic_memcpy =
		    LLVMGetIntrinsicDeclaration(ctx->llvm_module, id, pt, static_arrlenu(pt));
		ctx->intrinsic_memcpy_type =
		    llvm_intrinsic_get_type(ctx->llvm_cnt, id, pt, static_arrlenu(pt));

		bassert(ctx->intrinsic_memcpy && "Invalid memcpy intrinsic!");
	}
}

static void DI_init(struct ir_context *ctx) {
	arrsetcap(ctx->di_incomplete_types, 1024);
	const char   *producer    = "blc version " BL_VERSION;
	struct scope *gscope      = ctx->assembly->gscope;
	LLVMModuleRef llvm_module = ctx->assembly->llvm.module;

	// setup module flags for debug
	if (ctx->assembly->target->di == ASSEMBLY_DI_DWARF) {
		LLVMMetadataRef llvm_ver = LLVMValueAsMetadata(
		    LLVMConstInt(ctx->builtin_types->t_s32->llvm_type, BL_DWARF_VERSION, true));
		LLVMAddModuleFlag(
		    llvm_module, LLVMModuleFlagBehaviorWarning, "Debug Info Version", 18, llvm_ver);
	} else if (ctx->assembly->target->di == ASSEMBLY_DI_CODEVIEW) {
		LLVMMetadataRef llvm_ver = LLVMValueAsMetadata(
		    LLVMConstInt(ctx->builtin_types->t_s32->llvm_type, BL_CODE_VIEW_VERSION, true));
		LLVMAddModuleFlag(llvm_module, LLVMModuleFlagBehaviorWarning, "CodeView", 8, llvm_ver);
	}

	// create DI builder
	ctx->llvm_di_builder = LLVMCreateDIBuilder(llvm_module);

	// create dummy file used as DI global scope
	gscope->llvm_meta = LLVMDIBuilderCreateFile(ctx->llvm_di_builder,
	                                            ctx->assembly->target->name,
	                                            strlen(ctx->assembly->target->name),
	                                            ".",
	                                            1);

	const bool is_optimized = ctx->assembly->target->opt != ASSEMBLY_OPT_DEBUG;

	// create main compile unit
	LLVMDIBuilderCreateCompileUnit(ctx->llvm_di_builder,
	                               LLVMDWARFSourceLanguageC99,
	                               gscope->llvm_meta,
	                               producer,
	                               strlen(producer),
	                               is_optimized,
	                               NULL,
	                               0,
	                               1,
	                               "",
	                               0,
	                               LLVMDWARFEmissionFull,
	                               0,
	                               false,
	                               false,
	                               NULL,
	                               0,
	                               NULL,
	                               0);
	// llvm_di_create_compile_unit(ctx->llvm_di_builder, gscope->llvm_meta, producer);
}

static void DI_terminate(struct ir_context *ctx) {
	arrfree(ctx->di_incomplete_types);
	LLVMDisposeDIBuilder(ctx->llvm_di_builder);
}

static void DI_complete_types(struct ir_context *ctx)

{
	for (usize i = 0; i < arrlenu(ctx->di_incomplete_types); ++i) {
		DI_complete_type(ctx, ctx->di_incomplete_types[i]);
	}
}

// public
void ir_run(struct assembly *assembly) {
	zone();
	runtime_measure_begin(llvm);
	struct ir_context ctx;
	memset(&ctx, 0, sizeof(struct ir_context));
	ctx.assembly            = assembly;
	ctx.builtin_types       = &assembly->builtin_types;
	ctx.generate_debug_info = assembly->target->opt == ASSEMBLY_OPT_DEBUG ||
	                          assembly->target->opt == ASSEMBLY_OPT_RELEASE_WITH_DEBUG_INFO;
	ctx.llvm_cnt     = assembly->llvm.ctx;
	ctx.llvm_td      = assembly->llvm.TD;
	ctx.llvm_builder = llvm_create_builder_in_context(ctx.llvm_cnt);

	qsetcap(&ctx.incomplete_queue, 256);
	qsetcap(&ctx.queue, 256);

	tbl_init(ctx.gstring_cache, 2048);
	tbl_init(ctx.llvm_fn_cache, 2048);

	init_llvm_module(&ctx);

	if (ctx.generate_debug_info) {
		DI_init(&ctx);
	}

	ctx.llvm_const_i64_zero = LLVMConstInt(get_type(&ctx, ctx.builtin_types->t_u64), 0, false);
	ctx.llvm_const_i8_zero  = LLVMConstInt(get_type(&ctx, ctx.builtin_types->t_u8), 0, false);

	intrinsics_init(&ctx);
	for (usize i = 0; i < arrlenu(assembly->mir.exported_instrs); ++i) {
		qpush_back(&ctx.queue, assembly->mir.exported_instrs[i]);
	}
	process_queue(&ctx);
	emit_incomplete(&ctx);

	if (ctx.generate_debug_info) {
		DI_complete_types(&ctx);

		blog("DI finalize!");
		LLVMDIBuilderFinalize(ctx.llvm_di_builder);
	}

	if (assembly->target->verify_llvm) {
		char *llvm_error = NULL;
		if (LLVMVerifyModule(ctx.llvm_module, LLVMReturnStatusAction, &llvm_error)) {
			builder_warning("\nLLVM module not verified; error: \n%s", llvm_error);
		} else {
			builder_info("LLVM module verified without errors.");
		}
		LLVMDisposeMessage(llvm_error);
	}

	blog("Generated %d instructions.", ctx.emit_instruction_count);
	batomic_fetch_add_s32(&assembly->stats.llvm_ms, runtime_measure_end(llvm));

	if (!builder.options->do_cleanup_when_done) return_zone();

	LLVMDisposeBuilder(ctx.llvm_builder);
	if (ctx.generate_debug_info) {
		DI_terminate(&ctx);
	}

	qfree(&ctx.queue);
	qfree(&ctx.incomplete_queue);
	arrfree(ctx.incomplete_rtti);
	tbl_free(ctx.gstring_cache);
	tbl_free(ctx.llvm_fn_cache);

	return_zone();
}
