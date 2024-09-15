// =================================================================================================
// bl
//
// File:   docs.c
// Author: Martin Dorazil
// Date:   11/15/20
//
// Copyright 2020 Martin Dorazil
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

#include "builder.h"
#include "stb_ds.h"

#define H0(stream, str) fprintf(stream, "\n# %s\n", str)
#define H1(stream, str) fprintf(stream, "\n## %s\n", str)
#define H2(stream, str) fprintf(stream, "\n### %s\n", str)

#define INDENTATION_CHARS 4

#define CODE_BLOCK_BEGIN(stream) fprintf(stream, "\n```bl\n")
#define CODE_BLOCK_END(stream) fprintf(stream, "\n```\n\n")
#define CODE_INDENTED(stream, indentation)                               \
	if (indentation) {                                                   \
		fprintf(stream, "\n%*c", INDENTATION_CHARS *(indentation), ' '); \
	} else {                                                             \
		fprintf(stream, "\n");                                           \
	}                                                                    \
	(void)0

#define PUSH_IS_INLINE(ctx)                      \
	const bool _prev_is_inline = ctx->is_inline; \
	ctx->is_inline             = true

#define POP_IS_INLINE(ctx) ctx->is_inline = _prev_is_inline

#define PUSH_IS_MULTI_RETURN(ctx)                  \
	const bool _prev_is_mr = ctx->is_multi_return; \
	ctx->is_multi_return   = true

#define POP_IS_MULTI_RETURN(ctx) ctx->is_multi_return = _prev_is_mr

struct context {
	struct unit *unit;
	FILE        *stream;
	bool         is_inline;
	bool         is_multi_return;
	str_buf_t    section_variants;
	str_buf_t    section_members;
	char        *output_directory;
	s32          indentation;
};

static void append_section(struct context *ctx, const char *name, const str_t content);

static void doc(struct context *ctx, struct ast *node);
static void doc_unit(struct context *ctx, struct unit *unit);
static void doc_ublock(struct context *ctx, struct ast *block);
static void doc_decl_entity(struct context *ctx, struct ast *decl);
static void doc_decl_arg(struct context *ctx, struct ast *decl);
static void doc_decl_variant(struct context *ctx, struct ast *decl);
static void doc_decl_member(struct context *ctx, struct ast *decl);
static void doc_type_ptr(struct context *ctx, struct ast *type);
static void doc_type_fn(struct context *ctx, struct ast *type);
static void doc_type_enum(struct context *ctx, struct ast *type);
static void doc_type_struct(struct context *ctx, struct ast *type);
static void doc_type_slice(struct context *ctx, struct ast *type);
static void doc_type_dynarray(struct context *ctx, struct ast *type);
static void doc_type_vargs(struct context *ctx, struct ast *type);
static void doc_type_poly(struct context *ctx, struct ast *type);
static void doc_expr_lit_fn_group(struct context *ctx, struct ast *lit);

void append_section(struct context *ctx, const char *name, const str_t content) {
	H2(ctx->stream, name);
	fprintf(ctx->stream, "%.*s", content.len, content.ptr);
}

void doc_ublock(struct context *ctx, struct ast *block) {
	for (usize i = 0; i < arrlenu(block->data.ublock.nodes); ++i) {
		doc(ctx, block->data.ublock.nodes[i]);
	}
}

void doc_decl_entity(struct context *ctx, struct ast *decl) {
	struct ast *ident = decl->data.decl.name;
	struct ast *type  = decl->data.decl.type;
	struct ast *value = decl->data.decl_entity.value;
	if (!ident) return;
	const str_t text       = decl->docs;
	const str_t name       = ident->data.ident.id.str;
	const bool  is_mutable = decl->data.decl_entity.mut;

	// Symbols ignored by documentation.
	if (name.ptr[0] == '_') return;

	// @Performance: we can do it better I guess.
	if (text.len && (str_match(text, cstr("@INCOMPLETE")) || str_match(text, cstr("@Incomplete")) ||
	                 str_match(text, cstr("@incomplete")))) {
		builder_msg(MSG_WARN, 0, ident->location, CARET_WORD, "Found incomplete documentation!");
	}

	if (!decl->owner_scope) return;
	if (decl->owner_scope->kind != SCOPE_GLOBAL && decl->owner_scope->kind != SCOPE_NAMED) return;

	const str_t scope_name =
	    decl->owner_scope->kind == SCOPE_NAMED ? decl->owner_scope->name : str_empty;

	str_buf_t full_name = get_tmp_str();
	if (scope_name.len) {
		str_buf_append_fmt(&full_name, "{str}.{str}", scope_name, name);
	} else {
		str_buf_append_fmt(&full_name, "{str}", name);
	}

	H1(ctx->stream, str_to_c(full_name));
	put_tmp_str(full_name);
	CODE_BLOCK_BEGIN(ctx->stream);
	fprintf(ctx->stream, "%.*s :", name.len, name.ptr);
	if (type) {
		fprintf(ctx->stream, " ");
		doc(ctx, type);
		fprintf(ctx->stream, " ");
	}
	fprintf(ctx->stream, "%s", is_mutable ? "= " : ": ");
	doc(ctx, value);

	if (decl->data.decl.flags & FLAG_EXTERN) fprintf(ctx->stream, " #extern");
	if (decl->data.decl.flags & FLAG_INLINE) fprintf(ctx->stream, " #inline");
	CODE_BLOCK_END(ctx->stream);
	if (text.len) fprintf(ctx->stream, "%.*s\n\n", text.len, text.ptr);

	if (ctx->section_variants.len) {
		append_section(ctx, "Variants", str_buf_view(ctx->section_variants));
		str_buf_clr(&ctx->section_variants);
	}

	if (ctx->section_members.len) {
		append_section(ctx, "Members", str_buf_view(ctx->section_members));
		str_buf_clr(&ctx->section_members);
	}

	fprintf(ctx->stream, "\n\n*File: %.*s*\n\n", ctx->unit->filename.len, ctx->unit->filename.ptr);
}

void doc_decl_arg(struct context *ctx, struct ast *decl) {
	struct ast *ident = decl->data.decl.name;
	struct ast *type  = decl->data.decl.type;
	struct ast *value = decl->data.decl_arg.value;
	const str_t name  = ident ? ident->data.ident.id.str : str_empty;
	if (type && value) {
		fprintf(ctx->stream, "%.*s : ", name.len, name.ptr);
		doc(ctx, type);
		fprintf(ctx->stream, ": ");
		doc(ctx, value);
	} else if (type && !value) {
		fprintf(ctx->stream, "%.*s: ", name.len, name.ptr);
		doc(ctx, type);
	} else if (value) {
		fprintf(ctx->stream, "%.*s :: ", name.len, name.ptr);
		doc(ctx, value);
	}
}

void doc_decl_variant(struct context *ctx, struct ast *decl) {
	struct ast *ident = decl->data.decl.name;
	struct ast *value = decl->data.decl_variant.value;
	if (ident) {
		const str_t name = ident->data.ident.id.str;
		fprintf(ctx->stream, "%.*s", name.len, name.ptr);
		if (decl->docs.len) {
			str_buf_append_fmt(&ctx->section_variants, "* `{str}` - {str}\n", name, decl->docs);
		}
	}
	if (value && value->kind == AST_EXPR_LIT_INT) {
		fprintf(ctx->stream, " = ");
		doc(ctx, value);
	}
}

void doc_decl_member(struct context *ctx, struct ast *decl) {
	struct ast *ident = decl->data.decl.name;
	struct ast *type  = decl->data.decl.type;
	if (ident) {
		const str_t name = ident->data.ident.id.str;
		fprintf(ctx->stream, "%.*s: ", name.len, name.ptr);

		if (decl->docs.len) {
			str_buf_append_fmt(&ctx->section_members, "* `{str}` - {str}\n", name, decl->docs);
		}
	}
	doc(ctx, type);
}

void doc_type_fn(struct context *ctx, struct ast *type) {
	struct ast *ret_type = type->data.type_fn.ret_type;
	fprintf(ctx->stream, "fn (");
	PUSH_IS_INLINE(ctx);

	ast_nodes_t *args = type->data.type_fn.args;
	for (usize i = 0; i < sarrlenu(args); ++i) {
		struct ast *arg = sarrpeek(args, i);
		doc(ctx, arg);
		if (i + 1 < sarrlenu(args)) fprintf(ctx->stream, ", ");
	}
	fprintf(ctx->stream, ") ");
	PUSH_IS_MULTI_RETURN(ctx);
	if (ret_type) doc(ctx, ret_type);
	POP_IS_MULTI_RETURN(ctx);
	POP_IS_INLINE(ctx);
}

void doc_type_enum(struct context *ctx, struct ast *type) {
	fprintf(ctx->stream, "enum ");
	if (type->data.type_enm.type) {
		doc(ctx, type->data.type_enm.type);
		fprintf(ctx->stream, " ");
	}
	fprintf(ctx->stream, "{");
	ast_nodes_t *variants = type->data.type_enm.variants;
	ctx->indentation += 1;
	for (usize i = 0; i < sarrlenu(variants); ++i) {
		struct ast *variant = sarrpeek(variants, i);
		CODE_INDENTED(ctx->stream, ctx->indentation);
		doc(ctx, variant);
		fprintf(ctx->stream, ";");
	}
	ctx->indentation -= 1;
	fprintf(ctx->stream, "\n}");
}

void doc_type_struct(struct context *ctx, struct ast *type) {
	if (!ctx->is_multi_return)
		if (type->data.type_strct.is_union)
			fprintf(ctx->stream, "union {");
		else
			fprintf(ctx->stream, "struct {");
	else
		fprintf(ctx->stream, "(");

	ctx->indentation += 1;
	ast_nodes_t *members = type->data.type_strct.members;
	for (usize i = 0; i < sarrlenu(members); ++i) {
		struct ast *member = sarrpeek(members, i);
		if (ctx->is_multi_return) {
			doc(ctx, member);
			if (i + 1 < sarrlenu(members)) fprintf(ctx->stream, ", ");
		} else {
			CODE_INDENTED(ctx->stream, ctx->indentation);
			doc(ctx, member);
			fprintf(ctx->stream, ";");
		}
	}
	ctx->indentation -= 1;
	if (ctx->is_multi_return) {
		fprintf(ctx->stream, ")");
	} else {
		CODE_INDENTED(ctx->stream, ctx->indentation);
		fprintf(ctx->stream, "}");
	}
}

void doc_type_slice(struct context *ctx, struct ast *type) {
	struct ast *elem_type = type->data.type_slice.elem_type;
	fprintf(ctx->stream, "[]");
	doc(ctx, elem_type);
}

void doc_type_dynarray(struct context *ctx, struct ast *type) {
	struct ast *elem_type = type->data.type_dynarr.elem_type;
	fprintf(ctx->stream, "[..]");
	doc(ctx, elem_type);
}

void doc_ref(struct context *ctx, struct ast *ref) {
	struct ast *ident           = ref->data.ref.ident;
	struct ast *ident_namespace = ref->data.ref.next;
	if (ident_namespace) {
		doc(ctx, ident_namespace);
		// const char *name = ident_namespace->data.ident.id.str;
		fprintf(ctx->stream, ".");
	}
	const str_t name = ident->data.ident.id.str;
	fprintf(ctx->stream, "%.*s", name.len, name.ptr);
}

void doc_type_ptr(struct context *ctx, struct ast *type) {
	struct ast *next_type = type->data.type_ptr.type;
	fprintf(ctx->stream, "*");
	doc(ctx, next_type);
}

void doc_type_vargs(struct context *ctx, struct ast *type) {
	struct ast *next_type = type->data.type_vargs.type;
	fprintf(ctx->stream, "...");
	doc(ctx, next_type);
}

void doc_type_poly(struct context *ctx, struct ast *type) {
	struct ast *ident = type->data.type_poly.ident;
	const str_t name  = ident->data.ident.id.str;
	fprintf(ctx->stream, "?%.*s", name.len, name.ptr);
}

void doc_expr_lit_fn_group(struct context *ctx, struct ast *lit) {
	ast_nodes_t *variants = lit->data.expr_fn_group.variants;
	fprintf(ctx->stream, "fn { ");
	ctx->indentation += 1;
	for (usize i = 0; i < sarrlenu(variants); ++i) {
		struct ast *variant = sarrpeek(variants, i);
		CODE_INDENTED(ctx->stream, ctx->indentation);
		doc(ctx, variant);
		if (i < sarrlenu(variants)) fprintf(ctx->stream, "; ");
	}
	ctx->indentation -= 1;
	fprintf(ctx->stream, "\n}");
}

void doc(struct context *ctx, struct ast *node) {
	if (!node) return;
	switch (node->kind) {
	case AST_UBLOCK:
		doc_ublock(ctx, node);
		break;
	case AST_DECL_ENTITY:
		doc_decl_entity(ctx, node);
		break;
	case AST_DECL_ARG:
		doc_decl_arg(ctx, node);
		break;
	case AST_DECL_VARIANT:
		doc_decl_variant(ctx, node);
		break;
	case AST_DECL_MEMBER:
		doc_decl_member(ctx, node);
		break;
	case AST_EXPR_LIT_FN:
		doc(ctx, node->data.expr_fn.type);
		break;
	case AST_EXPR_LIT_FN_GROUP:
		doc_expr_lit_fn_group(ctx, node);
		break;
	case AST_EXPR_TYPE:
		doc(ctx, node->data.expr_type.type);
		break;
	case AST_TYPE_ENUM:
		doc_type_enum(ctx, node);
		break;
	case AST_TYPE_STRUCT:
		doc_type_struct(ctx, node);
		break;
	case AST_TYPE_PTR:
		doc_type_ptr(ctx, node);
		break;
	case AST_REF:
		doc_ref(ctx, node);
		break;
	case AST_TYPE_FN:
		doc_type_fn(ctx, node);
		break;
	case AST_TYPE_SLICE:
		doc_type_slice(ctx, node);
		break;
	case AST_TYPE_DYNARR:
		doc_type_dynarray(ctx, node);
		break;
	case AST_TYPE_VARGS:
		doc_type_vargs(ctx, node);
		break;
	case AST_TYPE_POLY:
		doc_type_poly(ctx, node);
		break;
	case AST_EXPR_UNARY:
		switch (node->data.expr_unary.kind) { // @Incomplete
		case UNOP_NEG:
			fprintf(ctx->stream, "-");
			break;
		default:
			break;
		}
		doc(ctx, node->data.expr_unary.next);
		break;
	case AST_EXPR_LIT_INT:
		fprintf(ctx->stream, "%llu", node->data.expr_integer.val);
		break;
	case AST_EXPR_LIT_BOOL:
		fprintf(ctx->stream, "%s", node->data.expr_boolean.val ? "true" : "false");
		break;
	case AST_EXPR_LIT_FLOAT:
		fprintf(ctx->stream, "%f", node->data.expr_float.val);
		break;
	case AST_EXPR_LIT_DOUBLE:
		fprintf(ctx->stream, "%f", node->data.expr_double.val);
		break;
	case AST_EXPR_NULL:
		fprintf(ctx->stream, "null");
		break;
	case AST_CALL_LOC:
		fprintf(ctx->stream, "#call_location");
		break;
	case AST_EXPR_LIT_STRING:
		fprintf(ctx->stream,
		        "\"%.*s\"",
		        node->data.expr_string.val.len,
		        node->data.expr_string.val.ptr);
		break;

	case AST_LOAD:
	case AST_PRIVATE:
	case AST_LINK:
	case AST_IMPORT:
	case AST_SCOPE:
		break;
	default:
		bwarn("Missing doc generation for AST node '%s'.", ast_get_name(node));
		break;
	}
}

void doc_unit(struct context *ctx, struct unit *unit) {
	if (unit->filename.len) return;
	str_buf_t unit_name = get_tmp_str();
	str_t     name      = unit->filename;
	name.len -= 3;
	bassert(name.len > 0);
	str_buf_append_fmt(&unit_name, "{str}", name);
	ctx->unit = unit;

	// write unit global docs
	str_buf_t export_file = get_tmp_str();
	str_buf_append_fmt(&export_file, "{s}/{str}.md", ctx->output_directory, unit_name);
	FILE *f = fopen(str_to_c(export_file), "w");
	if (f == NULL) {
		builder_error("Cannot open file '%s'", export_file);
		goto DONE;
	}

	ctx->stream = f;
	if (unit->ast->docs.len) {
		const str_t docs = unit->ast->docs;
		fprintf(f, "%.*s\n", docs.len, docs.ptr);
	} else {
		H0(f, str_to_c(unit->filename));
	}
	doc(ctx, unit->ast);
	fclose(f);

DONE:
	put_tmp_str(export_file);
	put_tmp_str(unit_name);
}

void docs_run(struct assembly *assembly) {
	zone();
	struct context ctx;
	memset(&ctx, 0, sizeof(struct context));
	str_buf_setcap(&ctx.section_variants, 128);
	str_buf_setcap(&ctx.section_members, 128);
	ctx.output_directory = builder.options->doc_out_dir;

	// prepare output directory
	if (!dir_exists(ctx.output_directory)) create_dir(ctx.output_directory);

	for (usize i = 0; i < arrlenu(assembly->units); ++i) {
		struct unit *unit = assembly->units[i];
		doc_unit(&ctx, unit);
	}

	// cleanup
	str_buf_free(&ctx.section_variants);
	str_buf_free(&ctx.section_members);

	builder_info("Documentation written into '%s' directory.", ctx.output_directory);
	return_zone();
}
