#include "mir_printer.h"
#include "assembly.h"
#include "ast.h"
#include "builder.h"
#include "mir.h"
#include "stb_ds.h"

#ifdef BL_DEBUG
#define PRINT_ANALYZED_COMPTIMES true
#else
#define PRINT_ANALYZED_COMPTIMES false
#endif

struct context {
	struct assembly *assembly;
	FILE            *stream;
};

static void print_comptime_value_or_id(struct context *ctx, struct mir_instr *instr);

static inline void
print_type(struct context *ctx, struct mir_type *type, bool aligned, bool prefer_name) {
	str_buf_t type_name = mir_type2str(type, prefer_name);
	if (aligned) {
		fprintf(ctx->stream, "%16s", str_buf_to_c(type_name));
	} else {
		fprintf(ctx->stream, "%s", str_buf_to_c(type_name));
	}
	put_tmp_str(type_name);
}

static inline void print_instr_head(struct context *ctx, struct mir_instr *instr, const char *name) {
	if (!instr) return;

#ifdef BL_DEBUG
	if (instr->ref_count == MIR_NO_REF_COUNTING) {
		fprintf(ctx->stream, "    %%%-8llu (-)", (unsigned long long)instr->id);
	} else {
		fprintf(ctx->stream, "    %%%-8llu (%d)", (unsigned long long)instr->id, instr->ref_count);
	}
#else
	fprintf(ctx->stream, "    %%%-8llu", (unsigned long long)instr->id);
#endif
	print_type(ctx, instr->value.type, true, true);
	fprintf(ctx->stream, " %s ", name);
}

static inline void print_flags(struct context *ctx, u32 flags) {
	if (flags == 0) return;

	if (isflag(flags, FLAG_EXTERN)) fprintf(ctx->stream, "#extern");
	if (isflag(flags, FLAG_COMPILER)) fprintf(ctx->stream, " #compiler");
	if (isflag(flags, FLAG_TEST_FN)) fprintf(ctx->stream, " #test");
	if (isflag(flags, FLAG_INLINE)) fprintf(ctx->stream, " #inline");
	if (isflag(flags, FLAG_NO_INLINE)) fprintf(ctx->stream, " #noinline");

	fprintf(ctx->stream, " ");
}

#define print_const_value(C, V) _print_const_value((C), (V)->type, (V)->data)

static inline void
_print_const_value(struct context *ctx, struct mir_type *type, vm_stack_ptr_t value) {
	if (!type) return;
	if (!value) {
		fprintf(ctx->stream, "<NULL>");
		return;
	}

	switch (type->kind) {
	case MIR_TYPE_ENUM:
	case MIR_TYPE_INT: {
		if (type->data.integer.is_signed) {
			switch (type->store_size_bytes) {
			case 1:
				fprintf(ctx->stream, "%d", vm_read_as(s8, value));
				break;
			case 2:
				fprintf(ctx->stream, "%d", vm_read_as(s16, value));
				break;
			case 4:
				fprintf(ctx->stream, "%d", vm_read_as(s32, value));
				break;
			case 8:
				fprintf(ctx->stream, "%lld", vm_read_as(s64, value));
				break;
			default:
				fprintf(ctx->stream, "<INVALID>");
			}
		} else {
			switch (type->store_size_bytes) {
			case 1:
				fprintf(ctx->stream, "%u", vm_read_as(u8, value));
				break;
			case 2:
				fprintf(ctx->stream, "%u", vm_read_as(u16, value));
				break;
			case 4:
				fprintf(ctx->stream, "%u", vm_read_as(u32, value));
				break;
			case 8:
				fprintf(ctx->stream, "%llu", vm_read_as(u64, value));
				break;
			default:
				fprintf(ctx->stream, "<INVALID>");
			}
		}
		break;
	}

	case MIR_TYPE_REAL:
		switch (type->store_size_bytes) {
		case 4:
			fprintf(ctx->stream, "%f", vm_read_as(f32, value));
			break;
		case 8:
			fprintf(ctx->stream, "%f", vm_read_as(f64, value));
			break;
		default:
			fprintf(ctx->stream, "<INVALID>");
		}
		break;
		break;

	case MIR_TYPE_BOOL: {
		bool b = vm_read_as(bool, value);
		fprintf(ctx->stream, "%s", b ? "true" : "false");
		break;
	}

	case MIR_TYPE_TYPE: {
		struct mir_type *type2 = vm_read_as(struct mir_type *, value);
		print_type(ctx, type2, false, false);
		break;
	}

	case MIR_TYPE_PTR: {
		if (mir_deref_type(type)->kind == MIR_TYPE_FN) {
			struct mir_fn *fn = vm_read_as(struct mir_fn *, value);

			if (fn) {
				const str_t linkage_name = fn->linkage_name;
				fprintf(ctx->stream, "&" STR_FMT, STR_ARG(linkage_name));
			} else {
				fprintf(ctx->stream, "null");
			}
		} else {
			vm_stack_ptr_t ptr = vm_read_as(vm_stack_ptr_t, value);
			fprintf(ctx->stream, "%p", (void *)ptr);
		}
		break;
	}

	case MIR_TYPE_NULL:
		fprintf(ctx->stream, "null");
		break;

	case MIR_TYPE_STRING:
		fprintf(ctx->stream, "{");

		struct mir_type *elem_type = mir_get_struct_elem_type(type, 0);
		ptrdiff_t        offset    = vm_get_struct_elem_offset(ctx->assembly, type, 0);
		_print_const_value(ctx, elem_type, value + offset);

		fprintf(ctx->stream, ",\"");

		offset                 = vm_get_struct_elem_offset(ctx->assembly, type, 1);
		vm_stack_ptr_t str_ptr = value + offset;
		str_ptr                = VM_STACK_PTR_DEREF(str_ptr);
		if (str_ptr) {
			str_buf_t tmp = get_tmp_str();
			char      c;
			while ((c = *(str_ptr++))) {
				switch (c) {
				case '\n':
					str_buf_append(&tmp, cstr("\\n"));
					break;
				default: {
					char character[1] = {c};
					str_buf_append(&tmp, make_str(character, 1));
				}
				}
			}
			fprintf(ctx->stream, STR_FMT "\"}", STR_ARG(tmp));
			put_tmp_str(tmp);
		} else {
			fprintf(ctx->stream, "(null)\"}");
		}
		break;

	case MIR_TYPE_DYNARR:
	case MIR_TYPE_SLICE:
	case MIR_TYPE_VARGS:
	case MIR_TYPE_STRUCT: {
		fprintf(ctx->stream, "{");
		mir_members_t *members = type->data.strct.members;
		for (usize i = 0; i < sarrlenu(members); ++i) {
			struct mir_member *it          = sarrpeek(members, i);
			struct mir_type   *member_type = it->type;
			const ptrdiff_t    offset2     = vm_get_struct_elem_offset(ctx->assembly, type, (u32)i);
			_print_const_value(ctx, member_type, value + offset2);
			if (i < sarrlenu(members) - 1) fprintf(ctx->stream, ",");
		}
		fprintf(ctx->stream, "}");
		break;
	}

	case MIR_TYPE_ARRAY: {
		fprintf(ctx->stream, "[");

		struct mir_type *elem_type2 = type->data.array.elem_type;
		for (u32 i = 0; i < (u32)type->data.array.len; ++i) {
			const ptrdiff_t offset2 = vm_get_array_elem_offset(type, i);
			_print_const_value(ctx, elem_type2, value + offset2);
			if (i < type->data.array.len - 1) fprintf(ctx->stream, ",");
		}

		fprintf(ctx->stream, "]");
		break;
	}

	case MIR_TYPE_FN: {
		struct mir_fn *fn = vm_read_as(struct mir_fn *, value);
		bmagic_assert(fn);
		const str_t name = fn->full_name;
		fprintf(ctx->stream, "@" STR_FMT, STR_ARG(name));
		break;
	}

	case MIR_TYPE_NAMED_SCOPE: {
		struct scope *scope = vm_read_as(struct scope *, value);
		bmagic_assert(scope);
		const str_t name = scope->name;
		fprintf(ctx->stream, "[scope] " STR_FMT, STR_ARG(name));
		break;
	}

	default:
		fprintf(ctx->stream, "<CANNOT READ VALUE>");
	}
}

static void print_instr_set_initializer(struct context *ctx, struct mir_instr_set_initializer *si);
static void print_instr_toany(struct context *ctx, struct mir_instr_to_any *toany);
static void print_instr_phi(struct context *ctx, struct mir_instr_phi *phi);
static void print_instr_cast(struct context *ctx, struct mir_instr_cast *cast);
static void print_instr_sizeof(struct context *ctx, struct mir_instr_sizeof *szof);
static void print_instr_type_info(struct context *ctx, struct mir_instr_type_info *type_info);
static void print_instr_typeof(struct context *ctx, struct mir_instr_typeof *type_of);
static void print_instr_alignof(struct context *ctx, struct mir_instr_alignof *szof);
static void print_instr_load(struct context *ctx, struct mir_instr_load *load);
static void print_instr_addrof(struct context *ctx, struct mir_instr_addrof *addrof);
static void print_instr_elem_ptr(struct context *ctx, struct mir_instr_elem_ptr *elem_ptr);
static void print_instr_member_ptr(struct context *ctx, struct mir_instr_member_ptr *member_ptr);
static void print_instr_cond_br(struct context *ctx, struct mir_instr_cond_br *cond_br);
static void print_instr_compound(struct context *ctx, struct mir_instr_compound *init);
static void print_instr_vargs(struct context *ctx, struct mir_instr_vargs *vargs);
static void print_instr_br(struct context *ctx, struct mir_instr_br *br);
static void print_instr_switch(struct context *ctx, struct mir_instr_switch *sw);
static void print_instr_unreachable(struct context *ctx, struct mir_instr_unreachable *unr);
static void print_instr_debugbreak(struct context *ctx, struct mir_instr_debugbreak *debug_break);
static void print_instr_fn_proto(struct context *ctx, struct mir_instr_fn_proto *fn_proto);
static void print_instr_fn_group(struct context *ctx, struct mir_instr_fn_group *group);
static void print_instr_type_fn(struct context *ctx, struct mir_instr_type_fn *type_fn);
static void print_instr_type_fn_group(struct context *ctx, struct mir_instr_type_fn_group *group);
static void print_instr_type_struct(struct context *ctx, struct mir_instr_type_struct *type_struct);
static void print_instr_type_enum(struct context *ctx, struct mir_instr_type_enum *type_enum);
static void print_instr_type_ptr(struct context *ctx, struct mir_instr_type_ptr *type_ptr);
static void print_instr_type_poly(struct context *ctx, struct mir_instr_type_poly *type_poly);
static void print_instr_type_array(struct context *ctx, struct mir_instr_type_array *type_array);
static void print_instr_type_slice(struct context *ctx, struct mir_instr_type_slice *type_slice);
static void print_instr_type_dynarr(struct context                *ctx,
                                    struct mir_instr_type_dyn_arr *type_dynarr);
static void print_instr_type_vargs(struct context *ctx, struct mir_instr_type_vargs *type_vargs);
static void print_instr_block(struct context *ctx, struct mir_instr_block *block);
static void print_instr_decl_var(struct context *ctx, struct mir_instr_decl_var *decl);
static void print_instr_decl_member(struct context *ctx, struct mir_instr_decl_member *decl);
static void print_instr_decl_variant(struct context *ctx, struct mir_instr_decl_variant *var);
static void print_instr_decl_arg(struct context *ctx, struct mir_instr_decl_arg *decl);
static void print_instr_const(struct context *ctx, struct mir_instr_const *ci);
static void print_instr_ret(struct context *ctx, struct mir_instr_ret *ret);
static void print_instr_store(struct context *ctx, struct mir_instr_store *store);
static void print_instr_binop(struct context *ctx, struct mir_instr_binop *binop);
static void print_instr_call(struct context *ctx, struct mir_instr_call *call);
static void print_instr_decl_ref(struct context *ctx, struct mir_instr_decl_ref *ref);
static void print_instr_unop(struct context *ctx, struct mir_instr_unop *unop);
static void print_instr_arg(struct context *ctx, struct mir_instr_arg *arg);
static void print_instr_test_cases(struct context *ctx, struct mir_instr_test_case *tc);
static void print_instr_call_loc(struct context *ctx, struct mir_instr_call_loc *loc);
static void print_instr_unroll(struct context *ctx, struct mir_instr_unroll *unroll);
static void print_instr_msg(struct context *ctx, struct mir_instr_msg *msg);
static void print_instr_using(struct context *ctx, struct mir_instr_using *using);
static void print_instr_designator(struct context *ctx, struct mir_instr_designator *designator);
static void print_instr(struct context *ctx, struct mir_instr *instr);

// impl
void print_comptime_value_or_id(struct context *ctx, struct mir_instr *instr) {
	if (!instr) {
		fprintf(ctx->stream, "<NULL>");
		return;
	}

	if (!instr->value.is_comptime || instr->state != MIR_IS_COMPLETE) {
		fprintf(ctx->stream, "%%%llu", (unsigned long long)instr->id);
		return;
	}

	// Value is compile time known constant.
	if (instr->kind == MIR_INSTR_DECL_REF) {
		const str_t s = ((struct mir_instr_decl_ref *)instr)->rid->str;
		fprintf(ctx->stream, STR_FMT, STR_ARG(s));
		return;
	}

	print_const_value(ctx, &instr->value);
}

void print_instr_type_fn(struct context *ctx, struct mir_instr_type_fn *type_fn) {
	print_instr_head(ctx, &type_fn->base, "const fn");
	fprintf(ctx->stream, "(");
	for (usize i = 0; i < sarrlenu(type_fn->args); ++i) {
		struct mir_instr *tmp = sarrpeek(type_fn->args, i);
		fprintf(ctx->stream, "%%%llu", (unsigned long long)tmp->id);
		if (i + 1 < sarrlenu(type_fn->args)) fprintf(ctx->stream, ", ");
	}

	fprintf(ctx->stream, ")");

	if (type_fn->ret_type)
		fprintf(ctx->stream, " %%%llu", (unsigned long long)type_fn->ret_type->id);
}

void print_instr_type_fn_group(struct context *ctx, struct mir_instr_type_fn_group *group) {
	print_instr_head(ctx, &group->base, "const fn");
	fprintf(ctx->stream, "{");
	for (usize i = 0; i < sarrlenu(group->variants); ++i) {
		struct mir_instr *tmp = sarrpeek(group->variants, i);
		fprintf(ctx->stream, "%%%llu", (unsigned long long)tmp->id);
		if (i + 1 < sarrlenu(group->variants)) fprintf(ctx->stream, ", ");
	}
	fprintf(ctx->stream, "}");
}

void print_instr_set_initializer(struct context *ctx, struct mir_instr_set_initializer *si) {
	print_instr_head(ctx, &si->base, "setinit");
	print_comptime_value_or_id(ctx, si->src);
	fprintf(ctx->stream, " -> ");

	struct mir_instr          *dest         = si->dest;
	struct mir_instr_decl_var *dest_var     = (struct mir_instr_decl_var *)dest;
	const str_t                linkage_name = dest_var->var->linkage_name;
	if (dest && linkage_name.len) {
		fprintf(ctx->stream, STR_FMT, STR_ARG(linkage_name));
	} else {
		print_comptime_value_or_id(ctx, dest);
	}
}

void print_instr_phi(struct context *ctx, struct mir_instr_phi *phi) {
	print_instr_head(ctx, &phi->base, "phi");

	struct mir_instr       *value;
	struct mir_instr_block *block;

	for (usize i = 0; i < phi->num; ++i) {
		value = phi->incoming_values[i];
		block = phi->incoming_blocks[i];

		if (!value || !block) continue;

		fprintf(ctx->stream, "[");
		print_comptime_value_or_id(ctx, value);
		fprintf(ctx->stream, ", ");
		fprintf(ctx->stream,
		        "%%" STR_FMT "_%llu",
		        STR_ARG(block->name),
		        (unsigned long long)block->base.id);
		fprintf(ctx->stream, "] ");
	}
}

void print_instr_toany(struct context *ctx, struct mir_instr_to_any *toany) {
	print_instr_head(ctx, &toany->base, "toany");
	print_comptime_value_or_id(ctx, toany->expr);
}

void print_instr_type_struct(struct context *ctx, struct mir_instr_type_struct *type_struct) {
	// @Inclomplete: Missing printing of tags and meta data.
	print_instr_head(ctx, &type_struct->base, "const struct");
	fprintf(ctx->stream, "{");

	mir_instrs_t *members = type_struct->members;
	for (usize i = 0; i < sarrlenu(members); ++i) {
		struct mir_instr *member = sarrpeek(members, i);
		print_comptime_value_or_id(ctx, member);
		if (i + 1 < sarrlenu(members)) fprintf(ctx->stream, ", ");
	}

	fprintf(ctx->stream, "}");
}

void print_instr_type_enum(struct context *ctx, struct mir_instr_type_enum *type_enum) {
	print_instr_head(ctx, &type_enum->base, "const enum");
	fprintf(ctx->stream, "{");

	mir_instrs_t *variants = type_enum->variants;
	for (usize i = 0; i < sarrlenu(variants); ++i) {
		struct mir_instr *variant = sarrpeek(variants, i);
		fprintf(ctx->stream, "%%%llu", (unsigned long long)variant->id);
		if (i + 1 < sarrlenu(variants)) fprintf(ctx->stream, ", ");
	}

	fprintf(ctx->stream, "}");
}

void print_instr_type_ptr(struct context *ctx, struct mir_instr_type_ptr *type_ptr) {
	print_instr_head(ctx, &type_ptr->base, "const");
	fprintf(ctx->stream, "*%%%llu", (unsigned long long)type_ptr->type->id);
}

void print_instr_type_poly(struct context *ctx, struct mir_instr_type_poly *type_poly) {
	print_instr_head(ctx, &type_poly->base, "const");
	if (type_poly->T_id) {
		const str_t s = type_poly->T_id->str;
		fprintf(ctx->stream, "?" STR_FMT, STR_ARG(s));
	} else {
		fprintf(ctx->stream, "?<INVALID>");
	}
}

void print_instr_type_array(struct context *ctx, struct mir_instr_type_array *type_array) {
	print_instr_head(ctx, &type_array->base, "const");
	fprintf(ctx->stream,
	        "[%%%llu]%%%llu",
	        (unsigned long long)type_array->len->id,
	        (unsigned long long)type_array->elem_type->id);
}

void print_instr_type_slice(struct context *ctx, struct mir_instr_type_slice *type_slice) {
	print_instr_head(ctx, &type_slice->base, "const");
	fprintf(ctx->stream, "[]%%%llu", (unsigned long long)type_slice->elem_type->id);
}

void print_instr_type_dynarr(struct context *ctx, struct mir_instr_type_dyn_arr *type_dynarr) {
	print_instr_head(ctx, &type_dynarr->base, "const");
	fprintf(ctx->stream, "[..]%%%llu", (unsigned long long)type_dynarr->elem_type->id);
}

void print_instr_type_vargs(struct context *ctx, struct mir_instr_type_vargs *type_vargs) {
	print_instr_head(ctx, &type_vargs->base, "const");
	if (!type_vargs->elem_type) return;
	fprintf(ctx->stream, "...%%%llu", (unsigned long long)type_vargs->elem_type->id);
}

void print_instr_cast(struct context *ctx, struct mir_instr_cast *cast) {
	switch (cast->op) {
	case MIR_CAST_NONE:
		print_instr_head(ctx, &cast->base, "nocast");
		break;
	case MIR_CAST_BITCAST:
		print_instr_head(ctx, &cast->base, "bitcast");
		break;
	case MIR_CAST_SEXT:
		print_instr_head(ctx, &cast->base, "sext");
		break;
	case MIR_CAST_ZEXT:
		print_instr_head(ctx, &cast->base, "zext");
		break;
	case MIR_CAST_TRUNC:
		print_instr_head(ctx, &cast->base, "trunc");
		break;
	case MIR_CAST_FPTOSI:
		print_instr_head(ctx, &cast->base, "fptosi");
		break;
	case MIR_CAST_FPTOUI:
		print_instr_head(ctx, &cast->base, "fptoui");
		break;
	case MIR_CAST_FPTRUNC:
		print_instr_head(ctx, &cast->base, "fptrunc");
		break;
	case MIR_CAST_FPEXT:
		print_instr_head(ctx, &cast->base, "fpext");
		break;
	case MIR_CAST_SITOFP:
		print_instr_head(ctx, &cast->base, "sitofp");
		break;
	case MIR_CAST_UITOFP:
		print_instr_head(ctx, &cast->base, "uitofp");
		break;
	case MIR_CAST_PTRTOINT:
		print_instr_head(ctx, &cast->base, "ptrtoint");
		break;
	case MIR_CAST_INTTOPTR:
		print_instr_head(ctx, &cast->base, "inttoptr");
		break;
	case MIR_CAST_PTRTOBOOL:
		print_instr_head(ctx, &cast->base, "ptrtobool");
		break;
	case MIR_CAST_INVALID:
		print_instr_head(ctx, &cast->base, "<INVALID CAST>");
		break;
	}

	fprintf(ctx->stream, "%%%llu", (unsigned long long)cast->expr->id);
}

void print_instr_compound(struct context *ctx, struct mir_instr_compound *init) {
	print_instr_head(ctx, &init->base, "compound");
	if (init->type) {
		print_comptime_value_or_id(ctx, init->type);
	} else {
		print_type(ctx, init->base.value.type, false, true);
	}

	fprintf(ctx->stream, " {");
	mir_instrs_t *values = init->values;
	if (values) {
		for (usize i = 0; i < sarrlenu(values); ++i) {
			struct mir_instr *value = sarrpeek(values, i);
			print_comptime_value_or_id(ctx, value);
			if (i < sarrlenu(values) - 1) fprintf(ctx->stream, ", ");
		}
	} else {
		fprintf(ctx->stream, "<ZERO INITIALIZER>");
	}
	fprintf(ctx->stream, "}");

	if (init->is_naked) fprintf(ctx->stream, " /* naked */");
}

void print_instr_vargs(struct context *ctx, struct mir_instr_vargs *vargs) {
	print_instr_head(ctx, &vargs->base, "vargs");
	print_type(ctx, vargs->type, false, true);

	fprintf(ctx->stream, " {");
	mir_instrs_t *values = vargs->values;
	if (values) {
		for (usize i = 0; i < sarrlenu(values); ++i) {
			struct mir_instr *value = sarrpeek(values, i);
			print_comptime_value_or_id(ctx, value);
			if (i < sarrlenu(values) - 1) fprintf(ctx->stream, ", ");
		}
	} else {
		fprintf(ctx->stream, "<INVALID VALUES>");
	}
	fprintf(ctx->stream, "}");
}

void print_instr_sizeof(struct context *ctx, struct mir_instr_sizeof *szof) {
	print_instr_head(ctx, &szof->base, "sizeof");
	fprintf(ctx->stream, " ");
	print_comptime_value_or_id(ctx, szof->expr);
}

void print_instr_type_info(struct context *ctx, struct mir_instr_type_info *type_info) {
	print_instr_head(ctx, &type_info->base, "typeinfo");
	print_comptime_value_or_id(ctx, type_info->expr);
}

void print_instr_typeof(struct context *ctx, struct mir_instr_typeof *type_of) {
	print_instr_head(ctx, &type_of->base, "typeof");
	print_comptime_value_or_id(ctx, type_of->expr);
}

void print_instr_alignof(struct context *ctx, struct mir_instr_alignof *szof) {
	print_instr_head(ctx, &szof->base, "alignof");
	fprintf(ctx->stream, " ");
	print_comptime_value_or_id(ctx, szof->expr);
}

void print_instr_elem_ptr(struct context *ctx, struct mir_instr_elem_ptr *elem_ptr) {
	print_instr_head(ctx, &elem_ptr->base, "elemptr");
	fprintf(ctx->stream, "%%%llu[", (unsigned long long)elem_ptr->arr_ptr->id);
	print_comptime_value_or_id(ctx, elem_ptr->index);
	fprintf(ctx->stream, "]");
}

void print_instr_member_ptr(struct context *ctx, struct mir_instr_member_ptr *member_ptr) {
	print_instr_head(ctx, &member_ptr->base, "memberptr");
	if (!member_ptr->target_ptr) {
		fprintf(ctx->stream, "<UNKNOWN>.");
	} else {
		print_comptime_value_or_id(ctx, member_ptr->target_ptr);
		fprintf(ctx->stream, ".");
	}

	if (member_ptr->builtin_id == BUILTIN_ID_NONE) {
		if (member_ptr->member_ident) {
			const str_t s = member_ptr->member_ident->data.ident.id.str;
			fprintf(ctx->stream, STR_FMT, STR_ARG(s));
		} else {
			fprintf(ctx->stream, "<UNKNOWN>");
		}
	} else {
		switch (member_ptr->builtin_id) {
		case BUILTIN_ID_ARR_LEN:
			fprintf(ctx->stream, "len");
			break;
		case BUILTIN_ID_ARR_PTR:
			fprintf(ctx->stream, "ptr");
			break;

		default:
			fprintf(ctx->stream, "<UNKNOWN>");
		}
	}
}

void print_instr_unop(struct context *ctx, struct mir_instr_unop *unop) {
	print_instr_head(ctx, &unop->base, "unop");

	const char *op = ast_unop_to_str(unop->op);
	fprintf(ctx->stream, "%s", op);
	print_comptime_value_or_id(ctx, unop->expr);
}

void print_instr_cond_br(struct context *ctx, struct mir_instr_cond_br *cond_br) {
	print_instr_head(ctx, &cond_br->base, "br");
	print_comptime_value_or_id(ctx, cond_br->cond);
	const str_t then_block = cond_br->then_block->name;
	const str_t else_block = cond_br->else_block->name;
	fprintf(ctx->stream,
	        " ? %%" STR_FMT "_%llu : %%" STR_FMT "_%llu",
	        STR_ARG(then_block),
	        (unsigned long long)cond_br->then_block->base.id,
	        STR_ARG(else_block),
	        (unsigned long long)cond_br->else_block->base.id);
}

void print_instr_arg(struct context *ctx, struct mir_instr_arg *arg) {
	print_instr_head(ctx, &arg->base, "arg");
	fprintf(ctx->stream, "$%u", arg->i);
}

void print_instr_unreachable(struct context *ctx, struct mir_instr_unreachable *unr) {
	print_instr_head(ctx, &unr->base, "unreachable");
}

void print_instr_debugbreak(struct context *ctx, struct mir_instr_debugbreak *debug_break) {
	print_instr_head(ctx, &debug_break->base, "debugbreak");
}

void print_instr_test_cases(struct context *ctx, struct mir_instr_test_case *tc) {
	print_instr_head(ctx, &tc->base, "testcases");
}

void print_instr_call_loc(struct context *ctx, struct mir_instr_call_loc *loc) {
	print_instr_head(ctx, &loc->base, "call_location");
}

void print_instr_unroll(struct context *ctx, struct mir_instr_unroll *unroll) {
	print_instr_head(ctx, &unroll->base, "unroll");
	print_comptime_value_or_id(ctx, unroll->src);
	if (unroll->index == UNROLL_LAST_INDEX) {
		fprintf(ctx->stream, ".LAST : ");
	} else {
		fprintf(ctx->stream, ".%d : ", unroll->index);
	}
	print_comptime_value_or_id(ctx, unroll->prev);
}

void print_instr_using(struct context *ctx, struct mir_instr_using *using) {
	print_instr_head(ctx, &using->base, "using");
	print_comptime_value_or_id(ctx, using->scope_expr);
}

void print_instr_designator(struct context *ctx, struct mir_instr_designator *designator) {
	print_instr_head(ctx, &designator->base, "designator");
	str_t name;
	if (designator->ident && designator->ident->kind == AST_IDENT) {
		name = designator->ident->data.ident.id.str;
	} else {
		name = cstr("<INVALID>");
	}
	fprintf(ctx->stream, STR_FMT " = ", STR_ARG(name));
	print_comptime_value_or_id(ctx, designator->value);
}

void print_instr_msg(struct context *ctx, struct mir_instr_msg *msg) {
	print_instr_head(ctx, &msg->base, "msg");
	print_comptime_value_or_id(ctx, msg->expr);
}

void print_instr_br(struct context *ctx, struct mir_instr_br *br) {
	print_instr_head(ctx, &br->base, "br");
	const str_t then_block = br->then_block->name;
	fprintf(ctx->stream,
	        "%%" STR_FMT "_%llu",
	        STR_ARG(then_block),
	        (unsigned long long)br->then_block->base.id);
}

void print_instr_switch(struct context *ctx, struct mir_instr_switch *sw) {
	print_instr_head(ctx, &sw->base, "switch");
	print_comptime_value_or_id(ctx, sw->value);
	fprintf(ctx->stream, " {");

	for (usize i = 0; i < sarrlenu(sw->cases); ++i) {
		struct mir_switch_case *c          = &sarrpeek(sw->cases, i);
		const str_t             block_name = c->block->name;
		print_comptime_value_or_id(ctx, c->on_value);
		fprintf(ctx->stream,
		        ": %%" STR_FMT "_%llu",
		        STR_ARG(block_name),
		        (unsigned long long)c->block->base.id);
		if (i < sarrlenu(sw->cases) - 1) fprintf(ctx->stream, "; ");
	}

	const str_t default_block_name = sw->default_block->name;
	fprintf(ctx->stream,
	        "} else %%" STR_FMT "_%llu",
	        STR_ARG(default_block_name),
	        (unsigned long long)sw->default_block->base.id);
}

void print_instr_load(struct context *ctx, struct mir_instr_load *load) {
	if (load->is_deref)
		print_instr_head(ctx, &load->base, "deref");
	else
		print_instr_head(ctx, &load->base, "load");

	print_comptime_value_or_id(ctx, load->src);
}

void print_instr_addrof(struct context *ctx, struct mir_instr_addrof *addrof) {
	print_instr_head(ctx, &addrof->base, "addrof");
	fprintf(ctx->stream, "%%%llu", (unsigned long long)addrof->src->id);
}

void print_instr_decl_var(struct context *ctx, struct mir_instr_decl_var *decl) {
	struct mir_var *var = decl->var;
	bassert(var);

	const str_t name = var->linkage_name.len ? var->linkage_name : cstr("<UNKNOWN>");

	if (isflag(var->iflags, MIR_VAR_GLOBAL)) {
		// global scope variable
		fprintf(ctx->stream, "\n@" STR_FMT " : ", STR_ARG(name));
		print_type(ctx, var->value.type, false, true);
		fprintf(ctx->stream, " %s ", isflag(var->iflags, MIR_VAR_MUTABLE) ? "=" : ":");

		if (var->value.is_comptime) {
			print_const_value(ctx, &var->value);
		} else {
			_print_const_value(ctx, var->value.type, var->vm_ptr.global);
		}
	} else {
		// local scope variable
		print_instr_head(ctx, &decl->base, "decl");

		fprintf(ctx->stream, STR_FMT " : ", STR_ARG(name));
		print_type(ctx, var->value.type, false, true);
		if (decl->init) {
			fprintf(ctx->stream, " %s ", isflag(var->iflags, MIR_VAR_MUTABLE) ? "=" : ":");
			print_comptime_value_or_id(ctx, decl->init);
		}
	}

	print_flags(ctx, var->flags);
}

void print_instr_decl_variant(struct context *ctx, struct mir_instr_decl_variant *var) {
	print_instr_head(ctx, &var->base, "declvariant");
	bassert(var->variant);

	struct mir_variant *variant = var->variant;
	bassert(variant);

	const str_t name = variant->id->str;
	fprintf(ctx->stream, STR_FMT, STR_ARG(name));

	if (var->base_type) {
		fprintf(ctx->stream, " : ");
		print_comptime_value_or_id(ctx, var->base_type);
	}

	if (var->value) {
		fprintf(ctx->stream, " : ");
		print_comptime_value_or_id(ctx, var->value);
	}
}

void print_instr_decl_arg(struct context *ctx, struct mir_instr_decl_arg *decl) {
	print_instr_head(ctx, &decl->base, "declarg");

	struct mir_arg *arg = decl->arg;
	bassert(arg);

	const str_t name = arg->id ? arg->id->str : cstr("-");
	fprintf(ctx->stream, STR_FMT " : ", STR_ARG(name));
	print_comptime_value_or_id(ctx, decl->type);

	if (arg->default_value) {
		fprintf(ctx->stream, " = ");
		print_comptime_value_or_id(ctx, arg->default_value);
	}
}

void print_instr_decl_member(struct context *ctx, struct mir_instr_decl_member *decl) {
	print_instr_head(ctx, &decl->base, "declmember");

	struct mir_member *member = decl->member;
	bassert(member);

	const str_t name = member->id->str;
	fprintf(ctx->stream, STR_FMT " : ", STR_ARG(name));
	print_comptime_value_or_id(ctx, decl->type);
}

void print_instr_decl_ref(struct context *ctx, struct mir_instr_decl_ref *ref) {
	print_instr_head(ctx, &ref->base, "declref");

	const str_t name = ref->rid->str;
	fprintf(ctx->stream, STR_FMT, STR_ARG(name));
	if (ref->accept_incomplete_type) fprintf(ctx->stream, " /* accept incomplete */");
}

void print_instr_decl_direct_ref(struct context *ctx, struct mir_instr_decl_direct_ref *ref) {
	print_instr_head(ctx, &ref->base, "declref");

	print_comptime_value_or_id(ctx, ref->ref);
	fprintf(ctx->stream, " /* direct */");
}

void print_instr_const(struct context *ctx, struct mir_instr_const *cnst) {
	print_instr_head(ctx, &cnst->base, "const");
	print_const_value(ctx, &cnst->base.value);
}

void print_instr_call(struct context *ctx, struct mir_instr_call *call) {
	print_instr_head(ctx, &call->base, "call");

	struct mir_fn *callee = mir_is_comptime(call->callee)
	                            ? MIR_CEV_READ_AS(struct mir_fn *, &call->callee->value)
	                            : NULL;
	if (callee) {
		const str_t name = callee->linkage_name;
		bassert(name.len);
		fprintf(ctx->stream, "@" STR_FMT, STR_ARG(name));
	} else {
		fprintf(ctx->stream, "%%%llu", (unsigned long long)call->callee->id);
	}

	fprintf(ctx->stream, "(");
	for (usize i = 0; i < sarrlenu(call->args); ++i) {
		struct mir_instr *tmp = sarrpeek(call->args, i);
		print_comptime_value_or_id(ctx, tmp);
		if (i < sarrlenu(call->args) - 1) fprintf(ctx->stream, ", ");
	}
	fprintf(ctx->stream, ")");
}

void print_instr_ret(struct context *ctx, struct mir_instr_ret *ret) {
	print_instr_head(ctx, &ret->base, "ret");
	if (ret->value) print_comptime_value_or_id(ctx, ret->value);
}

void print_instr_store(struct context *ctx, struct mir_instr_store *store) {
	print_instr_head(ctx, &store->base, "store");
	bassert(store->src);
	print_comptime_value_or_id(ctx, store->src);
	fprintf(ctx->stream, " -> %%%llu", (unsigned long long)store->dest->id);
	// print_comptime_value_or_id(ctx,store->dest);
}

void print_instr_binop(struct context *ctx, struct mir_instr_binop *binop) {
	print_instr_head(ctx, &binop->base, "binop");
	bassert(binop->lhs && binop->rhs);
	const char *op = ast_binop_to_str(binop->op);
	print_comptime_value_or_id(ctx, binop->lhs);
	fprintf(ctx->stream, " %s ", op);
	print_comptime_value_or_id(ctx, binop->rhs);
}

void print_instr_fn_group(struct context *ctx, struct mir_instr_fn_group *group) {
	print_instr_head(ctx, &group->base, "const fn");
	fprintf(ctx->stream, "{");
	mir_instrs_t *variants = group->variants;
	for (usize i = 0; i < sarrlenu(variants); ++i) {
		struct mir_instr *variant = sarrpeek(variants, i);
		fprintf(ctx->stream, "%%%llu", (unsigned long long)variant->id);
		if (i + 1 < sarrlenu(variants)) fprintf(ctx->stream, ", ");
	}
	fprintf(ctx->stream, "}");
}

void print_instr_block(struct context *ctx, struct mir_instr_block *block) {
	const bool is_global = !block->owner_fn;
	// if (block->base.prev || is_global) fprintf(ctx->stream, "\n");
#ifdef BL_DEBUG
	if (block->base.ref_count < 0) {
		fprintf(ctx->stream,
		        "%%" STR_FMT "_%llu (-):",
		        STR_ARG(block->name),
		        (unsigned long long)block->base.id);
	} else {
		fprintf(ctx->stream,
		        "%%" STR_FMT "_%llu (%u):",
		        STR_ARG(block->name),
		        (unsigned long long)block->base.id,
		        block->base.ref_count);
	}
#else
	fprintf(ctx->stream,
	        "%%" STR_FMT "_%llu:",
	        STR_ARG(block->name),
	        (unsigned long long)block->base.id);
#endif
	if (is_global) {
		fprintf(ctx->stream, " {\n");
	} else {
		if (block->is_unreachable)
			fprintf(ctx->stream, " /* NEVER REACHED */\n");
		else
			fprintf(ctx->stream, "\n");
	}
	struct mir_instr *tmp = block->entry_instr;
	while (tmp) {
		print_instr(ctx, tmp);
		tmp = tmp->next;
	}
	if (is_global) {
		fprintf(ctx->stream, "}");
	}
}

void print_instr_fn_proto(struct context *ctx, struct mir_instr_fn_proto *fn_proto) {
	struct mir_fn *fn = MIR_CEV_READ_AS(struct mir_fn *, &fn_proto->base.value);
	bassert(fn);

	fprintf(ctx->stream, "\n");
	if (fn_proto->base.state == MIR_IS_COMPLETE) fprintf(ctx->stream, "/* analyzed */\n");
	if (fn->linkage_name.len)
		fprintf(ctx->stream, "@" STR_FMT " ", STR_ARG(fn->linkage_name));
	else
		fprintf(ctx->stream, "@%llu ", (unsigned long long)fn_proto->base.id);

	if (fn->ref_count >= 0)
		fprintf(ctx->stream, "(%d) ", fn->ref_count);
	else
		fprintf(ctx->stream, "(-) ");

	fprintf(ctx->stream, ": ");
	print_type(ctx, fn->type, false, false);
	fprintf(ctx->stream, " : ");

	print_flags(ctx, fn->flags);

	struct mir_instr_block *tmp = fn->first_block;
	if (!tmp) return;
	fprintf(ctx->stream, "{\n");
	while (tmp) {
		print_instr(ctx, (struct mir_instr *)tmp);
		tmp = (struct mir_instr_block *)tmp->base.next;
	}
	fprintf(ctx->stream, "}");
}

// public
void print_instr(struct context *ctx, struct mir_instr *instr) {
#if !PRINT_ANALYZED_COMPTIMES
	if ((instr->owner_block || instr->kind == MIR_INSTR_BLOCK) &&
	    (instr->kind != MIR_INSTR_DECL_VAR) && instr->value.is_comptime &&
	    instr->state == MIR_IS_COMPLETE)
		return;
#endif

	switch (instr->kind) {
	case MIR_INSTR_BLOCK:
		print_instr_block(ctx, (struct mir_instr_block *)instr);
		break;
	case MIR_INSTR_INVALID:
		fprintf(ctx->stream, "INVALID");
		break;
	case MIR_INSTR_UNREACHABLE:
		print_instr_unreachable(ctx, (struct mir_instr_unreachable *)instr);
		break;
	case MIR_INSTR_DEBUGBREAK:
		print_instr_debugbreak(ctx, (struct mir_instr_debugbreak *)instr);
		break;
	case MIR_INSTR_DECL_VAR:
		print_instr_decl_var(ctx, (struct mir_instr_decl_var *)instr);
		break;
	case MIR_INSTR_DECL_VARIANT:
		print_instr_decl_variant(ctx, (struct mir_instr_decl_variant *)instr);
		break;
	case MIR_INSTR_DECL_MEMBER:
		print_instr_decl_member(ctx, (struct mir_instr_decl_member *)instr);
		break;
	case MIR_INSTR_DECL_ARG:
		print_instr_decl_arg(ctx, (struct mir_instr_decl_arg *)instr);
		break;
	case MIR_INSTR_CONST:
		print_instr_const(ctx, (struct mir_instr_const *)instr);
		break;
	case MIR_INSTR_LOAD:
		print_instr_load(ctx, (struct mir_instr_load *)instr);
		break;
	case MIR_INSTR_STORE:
		print_instr_store(ctx, (struct mir_instr_store *)instr);
		break;
	case MIR_INSTR_RET:
		print_instr_ret(ctx, (struct mir_instr_ret *)instr);
		break;
	case MIR_INSTR_BINOP:
		print_instr_binop(ctx, (struct mir_instr_binop *)instr);
		break;
	case MIR_INSTR_CALL:
		print_instr_call(ctx, (struct mir_instr_call *)instr);
		break;
	case MIR_INSTR_FN_PROTO:
		print_instr_fn_proto(ctx, (struct mir_instr_fn_proto *)instr);
		break;
	case MIR_INSTR_FN_GROUP:
		print_instr_fn_group(ctx, (struct mir_instr_fn_group *)instr);
		break;
	case MIR_INSTR_DECL_REF:
		print_instr_decl_ref(ctx, (struct mir_instr_decl_ref *)instr);
		break;
	case MIR_INSTR_TYPE_FN:
		print_instr_type_fn(ctx, (struct mir_instr_type_fn *)instr);
		break;
	case MIR_INSTR_TYPE_FN_GROUP:
		print_instr_type_fn_group(ctx, (struct mir_instr_type_fn_group *)instr);
		break;
	case MIR_INSTR_TYPE_STRUCT:
		print_instr_type_struct(ctx, (struct mir_instr_type_struct *)instr);
		break;
	case MIR_INSTR_TYPE_ARRAY:
		print_instr_type_array(ctx, (struct mir_instr_type_array *)instr);
		break;
	case MIR_INSTR_TYPE_SLICE:
		print_instr_type_slice(ctx, (struct mir_instr_type_slice *)instr);
		break;
	case MIR_INSTR_TYPE_DYNARR:
		print_instr_type_dynarr(ctx, (struct mir_instr_type_dyn_arr *)instr);
		break;
	case MIR_INSTR_TYPE_VARGS:
		print_instr_type_vargs(ctx, (struct mir_instr_type_vargs *)instr);
		break;
	case MIR_INSTR_TYPE_ENUM:
		print_instr_type_enum(ctx, (struct mir_instr_type_enum *)instr);
		break;
	case MIR_INSTR_COND_BR:
		print_instr_cond_br(ctx, (struct mir_instr_cond_br *)instr);
		break;
	case MIR_INSTR_BR:
		print_instr_br(ctx, (struct mir_instr_br *)instr);
		break;
	case MIR_INSTR_SWITCH:
		print_instr_switch(ctx, (struct mir_instr_switch *)instr);
		break;
	case MIR_INSTR_UNOP:
		print_instr_unop(ctx, (struct mir_instr_unop *)instr);
		break;
	case MIR_INSTR_ARG:
		print_instr_arg(ctx, (struct mir_instr_arg *)instr);
		break;
	case MIR_INSTR_ELEM_PTR:
		print_instr_elem_ptr(ctx, (struct mir_instr_elem_ptr *)instr);
		break;
	case MIR_INSTR_TYPE_PTR:
		print_instr_type_ptr(ctx, (struct mir_instr_type_ptr *)instr);
		break;
	case MIR_INSTR_TYPE_POLY:
		print_instr_type_poly(ctx, (struct mir_instr_type_poly *)instr);
		break;
	case MIR_INSTR_ADDROF:
		print_instr_addrof(ctx, (struct mir_instr_addrof *)instr);
		break;
	case MIR_INSTR_MEMBER_PTR:
		print_instr_member_ptr(ctx, (struct mir_instr_member_ptr *)instr);
		break;
	case MIR_INSTR_CAST:
		print_instr_cast(ctx, (struct mir_instr_cast *)instr);
		break;
	case MIR_INSTR_SIZEOF:
		print_instr_sizeof(ctx, (struct mir_instr_sizeof *)instr);
		break;
	case MIR_INSTR_ALIGNOF:
		print_instr_alignof(ctx, (struct mir_instr_alignof *)instr);
		break;
	case MIR_INSTR_COMPOUND:
		print_instr_compound(ctx, (struct mir_instr_compound *)instr);
		break;
	case MIR_INSTR_VARGS:
		print_instr_vargs(ctx, (struct mir_instr_vargs *)instr);
		break;
	case MIR_INSTR_TYPE_INFO:
		print_instr_type_info(ctx, (struct mir_instr_type_info *)instr);
		break;
	case MIR_INSTR_TYPEOF:
		print_instr_typeof(ctx, (struct mir_instr_typeof *)instr);
		break;
	case MIR_INSTR_PHI:
		print_instr_phi(ctx, (struct mir_instr_phi *)instr);
		break;
	case MIR_INSTR_TOANY:
		print_instr_toany(ctx, (struct mir_instr_to_any *)instr);
		break;
	case MIR_INSTR_DECL_DIRECT_REF:
		print_instr_decl_direct_ref(ctx, (struct mir_instr_decl_direct_ref *)instr);
		break;
	case MIR_INSTR_SET_INITIALIZER:
		print_instr_set_initializer(ctx, (struct mir_instr_set_initializer *)instr);
		break;
	case MIR_INSTR_TEST_CASES:
		print_instr_test_cases(ctx, (struct mir_instr_test_case *)instr);
		break;
	case MIR_INSTR_CALL_LOC:
		print_instr_call_loc(ctx, (struct mir_instr_call_loc *)instr);
		break;
	case MIR_INSTR_UNROLL:
		print_instr_unroll(ctx, (struct mir_instr_unroll *)instr);
		break;
	case MIR_INSTR_MSG:
		print_instr_msg(ctx, (struct mir_instr_msg *)instr);
		break;
	case MIR_INSTR_USING:
		print_instr_using(ctx, (struct mir_instr_using *)instr);
	case MIR_INSTR_DESIGNATOR:
		print_instr_designator(ctx, (struct mir_instr_designator *)instr);
		break;
	}

	bool has_comment = false;
	if (instr->value.is_comptime) {
		has_comment = true;
		fprintf(ctx->stream, " // comptime");
	}

	if (ctx->assembly->target->opt == ASSEMBLY_OPT_DEBUG) {
		if (instr->node && instr->node->location) {
			const struct location *loc = instr->node->location;
			fprintf(ctx->stream, " %s[" STR_FMT ":%d]", has_comment ? "" : "// ", STR_ARG(loc->unit->filename), loc->line);
		}
	}

	fprintf(ctx->stream, "\n");
}

void mir_print_instr(FILE *stream, struct assembly *assembly, struct mir_instr *instr) {
	if (!instr) return;
	struct context ctx = {.assembly = assembly, .stream = stream};
	print_instr(&ctx, instr);
}

void mir_print_fn(FILE *stream, struct assembly *assembly, struct mir_fn *fn) {
	if (!fn) return;
	if (!fn->prototype) return;
	struct context ctx = {.assembly = assembly, .stream = stream};
	print_instr(&ctx, fn->prototype);
}

void mir_print_assembly(FILE *stream, struct assembly *assembly) {
	struct context ctx = {.assembly = assembly, .stream = stream};
	for (usize i = 0; i < arrlenu(assembly->mir.global_instrs); ++i) {
		print_instr(&ctx, assembly->mir.global_instrs[i]);
	}
}
