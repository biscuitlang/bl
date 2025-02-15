// =================================================================================================
// bl
//
// File:   parser.c
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

#include "builder.h"
#include "table.h"
#include "tokens_inline_utils.h"
#include <setjmp.h>

#define report_error(code, tok, pos, format, ...) builder_msg(MSG_ERR, ERR_##code, &(tok)->location, (pos), (format), ##__VA_ARGS__)
#define report_warning(tok, pos, format, ...)     builder_msg(MSG_WARN, 0, &(tok)->location, (pos), (format), ##__VA_ARGS__)
#define report_note(tok, pos, format, ...)        builder_msg(MSG_ERR_NOTE, 0, &(tok)->location, (pos), (format), ##__VA_ARGS__)

#define scope_push(ctx, scope) arrput((ctx)->scope_stack, (scope))
#define scope_pop(ctx)         arrpop((ctx)->scope_stack)
#define scope_get(ctx)         arrlast((ctx)->scope_stack)
#define scope_set(ctx, scope)  (arrlast((ctx)->scope_stack) = (scope))

#define decl_push(ctx, decl) arrput((ctx)->decl_stack, decl)
#define decl_pop(ctx)        arrpop((ctx)->decl_stack)
#define decl_get(ctx)        (arrlenu((ctx)->decl_stack) ? arrlast((ctx)->decl_stack) : NULL)

#define block_push(ctx, block) arrput((ctx)->block_stack, block)
#define block_pop(ctx)         arrpop((ctx)->block_stack)
#define block_get(ctx)         (arrlenu((ctx)->block_stack) ? arrlast((ctx)->block_stack) : NULL)

#define consume_till(tokens, ...)                                 \
	{                                                             \
		enum sym _[] = {__VA_ARGS__};                             \
		tokens_consume_till2((tokens), static_arrlenu(_), &_[0]); \
	}                                                             \
	(void)0

enum hash_directive_flags {
#define HD_GEN(kind, name, flag) kind = flag,
#include "parser.def"
#undef HD_GEN
};

struct hash_directive_entry {
	hash_t                    hash;
	enum hash_directive_flags value;
};

struct context {
	hash_table(struct hash_directive_entry) hash_directive_table;

	struct assembly           *assembly;
	struct unit               *unit;
	struct arena              *ast_arena;
	struct arena              *sarr_arena;
	struct scope_thread_local *scope_thread_local;
	struct tokens             *tokens;
	struct string_cache      **string_cache;

	// tmps
	array(struct scope *) scope_stack;
	array(struct ast *) decl_stack;
	array(struct ast *) fn_type_stack;
	array(struct ast *) block_stack;
	bool        is_inside_loop;
	bool        is_inside_expression;
	struct ast *current_docs;
};

// helpers
// fw decls
static enum binop_kind sym_to_binop_kind(enum sym sm);
static enum unop_kind  sym_to_unop_kind(enum sym sm);
static bool            parse_docs(struct context *ctx);
static bool            parse_unit_docs(struct context *ctx);
static void            parse_ublock_content(struct context *ctx, struct ast *ublock);
static struct ast     *parse_hash_directive(struct context *ctx, s32 expected_mask, enum hash_directive_flags *satisfied, const bool is_in_expression);
static struct ast     *parse_unrecheable(struct context *ctx);
static struct ast     *parse_debugbreak(struct context *ctx);
static struct ast     *parse_ident_group(struct context *ctx);
static struct ast     *parse_single_block_stmt_or_expr(struct context *ctx, bool *out_require_semicolon);
static struct ast     *parse_block(struct context *ctx, enum scope_kind scope_kind);
static struct ast     *parse_decl(struct context *ctx);
static struct ast     *parse_decl_member(struct context *ctx, s32 index);
static struct ast     *parse_decl_arg(struct context *ctx, bool named);
static struct ast     *parse_decl_variant(struct context *ctx, struct ast *prev);
static struct ast     *parse_type(struct context *ctx);
static struct ast     *parse_ref(struct context *ctx);
static struct ast     *parse_ref_nested(struct context *ctx, struct ast *prev);
static struct ast     *parse_type_polymorph(struct context *ctx);
static struct ast     *parse_type_arr(struct context *ctx);
static struct ast     *parse_type_slice(struct context *ctx);
static struct ast     *parse_type_dynarr(struct context *ctx);
static struct ast     *parse_type_fn(struct context *ctx, bool named_args, bool create_scope);
static struct ast     *parse_type_fn_group(struct context *ctx);
static struct ast     *parse_type_fn_return(struct context *ctx);
static struct ast     *parse_type_struct(struct context *ctx);
static struct ast     *parse_type_enum(struct context *ctx);
static struct ast     *parse_type_ptr(struct context *ctx);
static struct ast     *parse_type_vargs(struct context *ctx);
static struct ast     *parse_stmt_return(struct context *ctx);
static struct ast     *parse_stmt_using(struct context *ctx);
static struct ast     *parse_stmt_if(struct context *ctx, bool is_static);
static struct ast     *parse_stmt_loop(struct context *ctx);
static struct ast     *parse_stmt_break(struct context *ctx);
static struct ast     *parse_stmt_continue(struct context *ctx);
static struct ast     *parse_stmt_defer(struct context *ctx);
static struct ast     *parse_stmt_switch(struct context *ctx);
static struct ast     *parse_stmt_case(struct context *ctx);

// EXPRESSIONS
static struct ast *parse_expr(struct context *ctx);
static struct ast *_parse_expr(struct context *ctx, s32 p);
static struct ast *parse_expr_atom(struct context *ctx);
static struct ast *parse_expr_primary(struct context *ctx);
static struct ast *parse_expr_unary(struct context *ctx);
static struct ast *parse_expr_binary(struct context *ctx, struct ast *lhs, struct ast *rhs, struct token *op);
static struct ast *parse_expr_addrof(struct context *ctx);
static struct ast *parse_expr_deref(struct context *ctx);
static struct ast *parse_expr_type(struct context *ctx);
static struct ast *parse_expr_ref(struct context *ctx);
static struct ast *parse_expr_nested(struct context *ctx);
static struct ast *parse_expr_null(struct context *ctx);
static struct ast *parse_expr_cast(struct context *ctx);
static struct ast *parse_expr_cast_auto(struct context *ctx);
static struct ast *parse_expr_lit(struct context *ctx);
static struct ast *parse_expr_lit_fn(struct context *ctx);
static struct ast *parse_expr_lit_fn_group(struct context *ctx);
static struct ast *parse_expr_test_cases(struct context *ctx);
static inline bool parse_semicolon(struct context *ctx);
static inline bool parse_semicolon_rq(struct context *ctx);
static inline bool hash_directive_to_flags(enum hash_directive_flags hd, u32 *out_flags);
static struct ast *parse_expr_call(struct context *ctx, struct ast *prev);
static struct ast *parse_expr_elem(struct context *ctx, struct ast *prev);
static struct ast *parse_expr_compound(struct context *ctx, struct ast *prev);

static inline union token_value get_token_value(struct context *ctx, struct token *token) {
	bassert(token && token->value_index < arrlenu(ctx->tokens->values));
	return ctx->tokens->values[token->value_index];
}

#define parse_ident(ctx) (tokens_peek((ctx)->tokens)->sym == SYM_IDENT ? _parse_ident(ctx) : NULL)
static inline struct ast *_parse_ident(struct context *ctx) {
	zone();
	struct token *tok_ident = tokens_consume(ctx->tokens);
	assert(tok_ident->sym == SYM_IDENT);
	struct ast *ident = ast_create_node(ctx->ast_arena, AST_IDENT, tok_ident, scope_get(ctx));
	id_init(&ident->data.ident.id, get_token_value(ctx, tok_ident).str);
	return_zone(ident);
}

// Set function type flavor flag of current parent function type being parsed. Asserts in case there
// is no current function type in the fn_type_stack.
static inline void set_parent_function_type_flavor(struct context               *ctx,
                                                   const enum ast_type_fn_flavor flavor) {
	bassert(
	    arrlenu(ctx->fn_type_stack) &&
	    "No parent function type to be set as polymorph type, this should be an compiler error!");
	for (usize i = arrlenu(ctx->fn_type_stack); i-- > 0;) {
		struct ast *fn_type = ctx->fn_type_stack[i];
		bassert(fn_type && fn_type->kind == AST_TYPE_FN);
		if (isflag(fn_type->data.type_fn.flavor, flavor)) return;
		setflag(fn_type->data.type_fn.flavor, flavor);
	}
}

static inline bool rq_semicolon_after_decl_entity(struct ast *node) {
	bassert(node);
	switch (node->kind) {
	case AST_EXPR_LIT_FN:
	case AST_EXPR_LIT_FN_GROUP:
	case AST_EXPR_TYPE:
		return false;
	default:
		return true;
	}
}

static inline str_t pop_docs(struct context *ctx) {
	str_t text = str_empty;
	if (ctx->current_docs) {
		text              = ctx->current_docs->data.docs.text;
		ctx->current_docs = NULL;
	}
	return text;
}

enum binop_kind sym_to_binop_kind(enum sym sm) {
	switch (sm) {
	case SYM_ASSIGN:
		return BINOP_ASSIGN;
	case SYM_PLUS_ASSIGN:
		return BINOP_ADD_ASSIGN;
	case SYM_MINUS_ASSIGN:
		return BINOP_SUB_ASSIGN;
	case SYM_ASTERISK_ASSIGN:
		return BINOP_MUL_ASSIGN;
	case SYM_SLASH_ASSIGN:
		return BINOP_DIV_ASSIGN;
	case SYM_PERCENT_ASSIGN:
		return BINOP_MOD_ASSIGN;
	case SYM_AND_ASSIGN:
		return BINOP_AND_ASSIGN;
	case SYM_OR_ASSIGN:
		return BINOP_OR_ASSIGN;
	case SYM_XOR_ASSIGN:
		return BINOP_XOR_ASSIGN;
	case SYM_PLUS:
		return BINOP_ADD;
	case SYM_MINUS:
		return BINOP_SUB;
	case SYM_ASTERISK:
		return BINOP_MUL;
	case SYM_SLASH:
		return BINOP_DIV;
	case SYM_PERCENT:
		return BINOP_MOD;
	case SYM_EQ:
		return BINOP_EQ;
	case SYM_NEQ:
		return BINOP_NEQ;
	case SYM_GREATER:
		return BINOP_GREATER;
	case SYM_LESS:
		return BINOP_LESS;
	case SYM_GREATER_EQ:
		return BINOP_GREATER_EQ;
	case SYM_LESS_EQ:
		return BINOP_LESS_EQ;
	case SYM_LOGIC_AND:
		return BINOP_LOGIC_AND;
	case SYM_LOGIC_OR:
		return BINOP_LOGIC_OR;
	case SYM_AND:
		return BINOP_AND;
	case SYM_OR:
		return BINOP_OR;
	case SYM_XOR:
		return BINOP_XOR;
	case SYM_SHR:
		return BINOP_SHR;
	case SYM_SHL:
		return BINOP_SHL;
	default:
		babort("unknown binop operation!!!");
	}
}

enum unop_kind sym_to_unop_kind(enum sym sm) {
	switch (sm) {
	case SYM_MINUS:
		return UNOP_NEG;
	case SYM_PLUS:
		return UNOP_POS;
	case SYM_NOT:
		return UNOP_NOT;
	case SYM_BIT_NOT:
		return UNOP_BIT_NOT;
	default:
		babort("unknown unop operation!!!");
	}
}

struct ast *parse_expr_ref(struct context *ctx) {
	zone();
	struct token *tok = tokens_peek(ctx->tokens);
	if (tok->sym != SYM_IDENT) return_zone(NULL);
	struct ast *ident = parse_ident(ctx);
	bassert(ident);
	struct ast *ref     = ast_create_node(ctx->ast_arena, AST_REF, tok, scope_get(ctx));
	ref->data.ref.ident = ident;
	return_zone(ref);
}

bool parse_docs(struct context *ctx) {
	struct token *tok_begin = tokens_consume_if(ctx->tokens, SYM_DCOMMENT);
	if (!tok_begin) return false;
	zone();
	str_t str_value = get_token_value(ctx, tok_begin).str;
	if (tokens_peek(ctx->tokens)->sym == SYM_DCOMMENT) {
		str_buf_t tmp = get_tmp_str();
		str_buf_append_fmt(&tmp, "{str}\n", str_value);

		struct token *tok;
		while ((tok = tokens_consume_if(ctx->tokens, SYM_DCOMMENT))) {
			const str_t str_value = get_token_value(ctx, tok).str;
			str_buf_append_fmt(&tmp, "{str}\n", str_value);
		}
		str_value = scdup2(ctx->string_cache, tmp);
		put_tmp_str(tmp);
	}

	struct ast *docs     = ast_create_node(ctx->ast_arena, AST_DOCS, tok_begin, scope_get(ctx));
	docs->data.docs.text = str_value;
	ctx->current_docs    = docs;
	return_zone(true);
}

bool parse_unit_docs(struct context *ctx) {
	struct token *tok_begin = tokens_peek(ctx->tokens);
	if (token_is_not(tok_begin, SYM_DGCOMMENT)) return false;
	zone();
	struct token *tok;
	while ((tok = tokens_consume_if(ctx->tokens, SYM_DGCOMMENT))) {
		if (ctx->unit->file_docs_cache.len) str_buf_append(&ctx->unit->file_docs_cache, cstr("\n"));
		const str_t str_value = get_token_value(ctx, tok).str;
		str_buf_append(&ctx->unit->file_docs_cache, str_value);
	}
	return_zone(true);
}

// Try to parse hash directive. List of enabled directives can be set by 'expected_mask',
// 'satisfied' is optional output set to parsed directive id if there is one.
struct ast *parse_hash_directive(struct context *ctx, s32 expected_mask, enum hash_directive_flags *satisfied, const bool is_in_expression) {
	zone();
#define set_satisfied(_hd)               \
	{                                    \
		if (satisfied) *satisfied = _hd; \
	}                                    \
	(void)0

	set_satisfied(HD_NONE);
	struct token *tok_hash = tokens_consume_if(ctx->tokens, SYM_HASH);
	if (!tok_hash) return_zone(NULL);
	// Special case for static if
	{
		struct ast *if_stmt = parse_stmt_if(ctx, true);
		if (if_stmt) {
			set_satisfied(HD_STATIC_IF);
			if (isnotflag(expected_mask, HD_STATIC_IF)) {
				builder_msg(MSG_ERR,
				            0,
				            if_stmt->location,
				            CARET_WORD,
				            "Static if is not allowed in this context.");
				return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_hash, scope_get(ctx)));
			}
			return_zone(if_stmt);
		}
	}

	struct token *tok_directive = tokens_consume(ctx->tokens);
	if (tok_directive->sym != SYM_IDENT) goto INVALID;

	const str_t  directive = get_token_value(ctx, tok_directive).str;
	const hash_t hash      = strhash(directive);
	const s64    index     = tbl_lookup_index(ctx->hash_directive_table, hash);
	if (index == -1) goto INVALID;
	const enum hash_directive_flags hd_flag = ctx->hash_directive_table[index].value;
	bassert(directive.len);

	if (isnotflag(expected_mask, hd_flag)) {
		report_error(UNEXPECTED_DIRECTIVE, tok_directive, CARET_WORD, "Unexpected directive.");
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_directive, scope_get(ctx)));
	}
	set_satisfied(hd_flag);

	switch (hd_flag) {
	case HD_NONE:
	case HD_STATIC_IF:
		babort("Invalid directive!");
	case HD_TEST_FN:
	case HD_BUILD_ENTRY:
	case HD_NO_INIT:
	case HD_FLAGS:
	case HD_INLINE:
	case HD_NO_INLINE:
	case HD_THREAD_LOCAL:
	case HD_ENTRY:
	case HD_MAYBE_UNUSED:
	case HD_COMPTIME:
	case HD_COMPILER: {
		// only flags
		return_zone(NULL);
	}

	case HD_LOAD: {
		struct token *tok_path = tokens_consume_if(ctx->tokens, SYM_STRING);
		if (!tok_path) {
			struct token *tok_err = tokens_peek(ctx->tokens);
			report_error(INVALID_DIRECTIVE,
			             tok_err,
			             CARET_WORD,
			             "Expected path \"some/path\" after 'load' directive.");
			return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_directive, scope_get(ctx)));
		}

		struct scope *current_scope = scope_get(ctx);
		bassert(current_scope);
		struct ast *load         = ast_create_node(ctx->ast_arena, AST_LOAD, tok_directive, current_scope);
		load->data.load.filepath = get_token_value(ctx, tok_path).str;
		if (ctx->assembly->target->kind != ASSEMBLY_DOCS) {
			assembly_add_unit(ctx->assembly, load->data.load.filepath, tok_path, current_scope, ctx->unit->module);
		}
		return_zone(load);
	}

	case HD_IMPORT: {
		struct scope *current_scope = scope_get(ctx);
		bassert(current_scope);

		struct token *tok_path = tokens_consume_if(ctx->tokens, SYM_STRING);
		if (!tok_path) {
			struct token *tok_err = tokens_peek(ctx->tokens);
			report_error(INVALID_DIRECTIVE,
			             tok_err,
			             CARET_WORD,
			             "Expected path \"some/path\" after 'import' directive.");
			return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_directive, current_scope));
		}

		struct ast *import = ast_create_node(ctx->ast_arena, AST_IMPORT, tok_directive, current_scope);
		if (ctx->assembly->target->kind != ASSEMBLY_DOCS) {
			struct module *module = assembly_import_module(ctx->assembly, get_token_value(ctx, tok_path).str, tok_path, current_scope);
			if (module) {
				import->data.import.module = module;
				if (!is_in_expression) {
					if (scope_is_subtree_of(current_scope, module->scope)) {
						report_error(INVALID_DIRECTIVE,
						             tok_directive,
						             CARET_WORD,
						             "Module cannot import itself.");
					} else {
						scope_inject(current_scope, module->scope);
					}
				} else {
					import->data.import.is_expression = true;
				}
			}
		}
		return_zone(import);
	}

	case HD_FILE: {
		struct ast *file =
		    ast_create_node(ctx->ast_arena, AST_EXPR_LIT_STRING, tok_directive, scope_get(ctx));
		file->data.expr_string.val = tok_directive->location.unit->filepath;
		return_zone(file);
	}

	case HD_LINE: {
		struct ast *line =
		    ast_create_node(ctx->ast_arena, AST_EXPR_LIT_INT, tok_directive, scope_get(ctx));
		line->data.expr_integer.val = tok_directive->location.line;
		return_zone(line);
	}

	case HD_BASE: {
		return_zone(parse_type(ctx));
	}

	case HD_TAG: {
		// Tags can contain one or move references separated by comma
		struct ast *expr = parse_expr(ctx);
		if (!expr) {
			struct token *tok_err = tokens_peek(ctx->tokens);
			report_error(EXPECTED_NAME, tok_err, CARET_WORD, "Expected tag expression.");
			return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_directive, scope_get(ctx)));
		}
		struct ast *tag    = ast_create_node(ctx->ast_arena, AST_TAG, tok_directive, scope_get(ctx));
		tag->data.tag.expr = expr;
		return_zone(tag);
	}

	case HD_CALL_LOC: {
		return_zone(ast_create_node(ctx->ast_arena, AST_CALL_LOC, tok_directive, scope_get(ctx)));
	}

	case HD_EXTERN: {
		// Extern flag extension could be linkage name as string
		struct token *tok_ext = tokens_consume_if(ctx->tokens, SYM_STRING);
		if (!tok_ext) return_zone(NULL);
		// Parse extension token.
		struct ast *ext = ast_create_node(ctx->ast_arena, AST_IDENT, tok_ext, scope_get(ctx));
		id_init(&ext->data.ident.id, get_token_value(ctx, tok_ext).str);
		return_zone(ext);
	}

	case HD_EXPORT: {
		// Export flag extension could be linkage name as string
		struct token *tok_ext = tokens_consume_if(ctx->tokens, SYM_STRING);
		if (!tok_ext) return_zone(NULL);
		// Parse extension token.
		struct ast *ext = ast_create_node(ctx->ast_arena, AST_IDENT, tok_ext, scope_get(ctx));
		id_init(&ext->data.ident.id, get_token_value(ctx, tok_ext).str);
		return_zone(ext);
	}

	case HD_OBSOLETE: {
		// Parse optional message.
		struct token *tok_message = tokens_consume_if(ctx->tokens, SYM_STRING);
		if (!tok_message) return_zone(NULL);
		struct ast *message           = ast_create_node(ctx->ast_arena, AST_EXPR_LIT_STRING, tok_message, scope_get(ctx));
		message->data.expr_string.val = get_token_value(ctx, tok_message).str;
		return_zone(message);
	}

	case HD_INTRINSIC: {
		// Intrinsic flag extension could be linkage name as string
		struct token *tok_ext = tokens_consume_if(ctx->tokens, SYM_STRING);
		if (!tok_ext) return_zone(NULL);
		// Parse extension token.
		struct ast *ext = ast_create_node(ctx->ast_arena, AST_IDENT, tok_ext, scope_get(ctx));
		id_init(&ext->data.ident.id, get_token_value(ctx, tok_ext).str);
		return_zone(ext);
	}

	case HD_PRIVATE: // @Deprecated 2024-12-12: Since version: 0.11.3
	case HD_SCOPE_PRIVATE: {
		struct scope *current_scope = scope_get(ctx);
		bassert(current_scope);
		struct scope *scope = ctx->unit->private_scope;
		if (!scope) {
			struct module *module       = ctx->unit->module;
			struct scope  *parent_scope = module ? module->private_scope : ctx->unit->parent_scope;
			bassert(parent_scope);
			scope = scope_create(ctx->scope_thread_local, SCOPE_PRIVATE, parent_scope, &tok_directive->location);
			scope_reserve(scope, 256);
			ctx->unit->private_scope = scope;
		}

		scope_set(ctx, scope);
		return_zone(ast_create_node(ctx->ast_arena, AST_PRIVATE, tok_directive, current_scope));
	}

	case HD_SCOPE_PUBLIC: {
		struct scope *current_scope = scope_get(ctx);
		bassert(current_scope);
		scope_set(ctx, ctx->unit->parent_scope);
		return_zone(ast_create_node(ctx->ast_arena, AST_PUBLIC, tok_directive, current_scope));
	}

	case HD_SCOPE_MODULE: {
		struct scope *current_scope = scope_get(ctx);
		bassert(current_scope);
		if (ctx->assembly->target->kind != ASSEMBLY_DOCS) {
			struct module *module = ctx->unit->module;
			if (!module) {
				report_error(UNEXPECTED_DIRECTIVE, tok_directive, CARET_WORD, "Module private scope cannot be created outside of module.");
				return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_directive, current_scope));
			}

			bassert(module->private_scope);
			scope_set(ctx, module->private_scope);
		} else {
			// Create fake private scope even for module private one, we need it just to skip generation of documentation
			// for private module stuff.
			struct scope *scope = ctx->unit->private_scope;
			if (!scope) {
				struct scope *parent_scope = ctx->unit->parent_scope;
				scope                      = scope_create(ctx->scope_thread_local, SCOPE_PRIVATE, parent_scope, &tok_directive->location);
				scope_reserve(scope, 256);
				ctx->unit->private_scope = scope;
			}

			scope_set(ctx, scope);
		}

		return_zone(ast_create_node(ctx->ast_arena, AST_MODULE_PRIVATE, tok_directive, current_scope));
	}

	case HD_ENABLE_IF: {
		struct ast *expr = parse_expr(ctx);
		if (!expr) {
			report_error(
			    INVALID_DIRECTIVE, tok_directive, CARET_AFTER, "Expected comptime expression.");
			return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_directive, scope_get(ctx)));
		}
		return_zone(expr);
	}
	}
INVALID:
	report_error(UNEXPECTED_DIRECTIVE, tok_directive, CARET_WORD, "Unknown directive.");
	return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_directive, scope_get(ctx)));
#undef set_satisfied
}

struct ast *parse_expr_compound(struct context *ctx, struct ast *prev) {
	zone();
	if (!tokens_is_seq(ctx->tokens, 2, SYM_DOT, SYM_LBLOCK)) return_zone(NULL);
	// eat .
	tokens_consume(ctx->tokens);
	// eat {
	struct token *tok_begin = tokens_consume(ctx->tokens);

	struct ast *type = prev;

	struct ast *compound              = ast_create_node(ctx->ast_arena, AST_EXPR_COMPOUND, tok_begin, scope_get(ctx));
	compound->data.expr_compound.type = type;

	// parse values
	bool        rq = false;
	struct ast *tmp;

NEXT:
	tmp = parse_expr(ctx);
	if (tmp) {
		if (!compound->data.expr_compound.values) {
			compound->data.expr_compound.values = arena_alloc(ctx->sarr_arena);
		}

		sarrput(compound->data.expr_compound.values, tmp);

		if (tokens_consume_if(ctx->tokens, SYM_COMMA)) {
			rq = true;
			goto NEXT;
		}
	} else if (rq) {
		struct token *tok_err = tokens_peek(ctx->tokens);
		if (tokens_peek_2nd(ctx->tokens)->sym == SYM_RBLOCK) {
			report_error(EXPECTED_NAME, tok_err, CARET_WORD, "Expected expression after comma ','.");
			return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_begin, scope_get(ctx)));
		}
	}

	struct token *tok = tokens_consume(ctx->tokens);
	if (tok->sym != SYM_RBLOCK) {
		report_error(MISSING_BRACKET,
		             tok,
		             CARET_WORD,
		             "Expected end of initialization list '}' or another expression "
		             "separated by comma.");
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_begin, scope_get(ctx)));
	}
	return_zone(compound);
}

struct ast *parse_expr_test_cases(struct context *ctx) {
	zone();
	struct token *tok_begin = tokens_consume_if(ctx->tokens, SYM_TESTCASES);
	if (!tok_begin) return_zone(NULL);
	struct ast *tc =
	    ast_create_node(ctx->ast_arena, AST_EXPR_TEST_CASES, tok_begin, scope_get(ctx));
	return_zone(tc);
}

struct ast *parse_expr_cast_auto(struct context *ctx) {
	zone();
	struct token *tok_begin = tokens_consume_if(ctx->tokens, SYM_CAST_AUTO);
	if (!tok_begin) return_zone(NULL);

	struct ast *cast               = ast_create_node(ctx->ast_arena, AST_EXPR_CAST, tok_begin, scope_get(ctx));
	cast->data.expr_cast.auto_cast = true;

	cast->data.expr_cast.next = _parse_expr(ctx, token_prec(tok_begin).priority);
	if (!cast->data.expr_cast.next) {
		struct token *tok = tokens_peek(ctx->tokens);
		report_error(EXPECTED_EXPR, tok, CARET_WORD, "Expected expression after auto cast.");
		tokens_consume_till(ctx->tokens, SYM_SEMICOLON);
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok, scope_get(ctx)));
	}

	return_zone(cast);
}

struct ast *parse_expr_cast(struct context *ctx) {
	zone();
	struct token *tok_begin = tokens_consume_if(ctx->tokens, SYM_CAST);
	if (!tok_begin) return_zone(NULL);

	struct token *tok = tokens_consume(ctx->tokens);
	if (!token_is(tok, SYM_LPAREN)) {
		report_error(MISSING_BRACKET, tok_begin, CARET_WORD, "Expected '(' after expression.");
		tokens_consume_till(ctx->tokens, SYM_SEMICOLON);
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_begin, scope_get(ctx)));
	}

	struct ast *cast          = ast_create_node(ctx->ast_arena, AST_EXPR_CAST, tok_begin, scope_get(ctx));
	cast->data.expr_cast.type = parse_type(ctx);
	if (!cast->data.expr_cast.type) {
		struct token *tok_err = tokens_peek(ctx->tokens);
		report_error(EXPECTED_TYPE, tok_err, CARET_WORD, "Expected type name as cast parameter.");
		tokens_consume_till(ctx->tokens, SYM_SEMICOLON);
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_err, scope_get(ctx)));
	}

	tok = tokens_consume(ctx->tokens);
	if (!token_is(tok, SYM_RPAREN)) {
		report_error(MISSING_BRACKET, tok, CARET_WORD, "Expected ')' after cast expression.");
		tokens_consume_till(ctx->tokens, SYM_SEMICOLON);
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok, scope_get(ctx)));
	}

	cast->data.expr_cast.next = _parse_expr(ctx, token_prec(tok_begin).priority);
	if (!cast->data.expr_cast.next) {
		tok = tokens_peek(ctx->tokens);
		report_error(EXPECTED_EXPR, tok, CARET_WORD, "Expected expression after cast.");
		tokens_consume_till(ctx->tokens, SYM_SEMICOLON);
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok, scope_get(ctx)));
	}

	return_zone(cast);
}

struct ast *parse_decl_member(struct context *ctx, s32 UNUSED(index)) {
	zone();
	struct token *tok_begin = tokens_peek(ctx->tokens);
	struct ast   *name      = NULL;
	struct ast   *type      = NULL;
	const bool    named     = tokens_peek_2nd(ctx->tokens)->sym == SYM_COLON;

	if (named) {
		name = parse_ident(ctx);
		if (!name) {
			builder_msg(MSG_ERR,
			            ERR_EXPECTED_TYPE,
			            &tok_begin->location,
			            CARET_AFTER,
			            "Expected member name.");
			tokens_consume(ctx->tokens);
		}
		bassert(tokens_current_is(ctx->tokens, SYM_COLON));
		tokens_consume(ctx->tokens);
		type = parse_type(ctx);
		if (!type) {
			builder_msg(MSG_ERR, ERR_EXPECTED_TYPE, name->location, CARET_AFTER, "Expected type.");
			return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_begin, scope_get(ctx)));
		}
	} else {
		type = parse_type(ctx);
		if (!type) return_zone(NULL);
	}

	if (!name) {
		bassert(index >= 0);
		// @Cleanup: Make this better?
		char      buf[64]; // More than enough.
		const s32 len = snprintf(buf, static_arrlenu(buf), "_%d", index);
		bassert(len >= 0 && len < (s32)static_arrlenu(buf) && "Buffer overflow!");
		const str_t ident = scdup2(ctx->string_cache, make_str(buf, len));
		name              = ast_create_node(ctx->ast_arena, AST_IDENT, tok_begin, scope_get(ctx));
		id_init(&name->data.ident.id, ident);
	}

	enum hash_directive_flags found_hd = HD_NONE;
	struct ast               *tag      = parse_hash_directive(ctx, HD_TAG, &found_hd, false);
	struct ast               *mem      = ast_create_node(ctx->ast_arena, AST_DECL_MEMBER, tok_begin, scope_get(ctx));

	mem->docs           = pop_docs(ctx);
	mem->data.decl.type = type;
	mem->data.decl.name = name;
	mem->data.decl.tag  = tag;
	return_zone(mem);
}

struct ast *parse_decl_arg(struct context *ctx, bool named) {
	zone();
	struct token *tok_begin = tokens_peek(ctx->tokens);
	struct ast   *name      = NULL;
	struct ast   *type      = NULL;
	struct ast   *value     = NULL;
	if (tokens_current_is(ctx->tokens, SYM_RPAREN)) return_zone(NULL);
	if (tokens_is_seq(ctx->tokens, 2, SYM_IDENT, SYM_COLON)) {
		// <name> :
		name = parse_ident(ctx);
		tokens_consume(ctx->tokens); // eat :
	} else if (named) {
		struct token *tok_err = tokens_peek_prev(ctx->tokens);
		builder_msg(MSG_ERR,
		            ERR_EXPECTED_NAME,
		            &tok_err->location,
		            CARET_AFTER,
		            "Expected argument name followed by colon.");

		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_err, scope_get(ctx)));
	}
	type = parse_type(ctx);
	// Parse optional default value expression.
	if (tokens_current_is(ctx->tokens, SYM_COLON)) {
		struct token *tok_err = tokens_consume(ctx->tokens);
		builder_msg(MSG_ERR,
		            ERR_INVALID_MUTABILITY,
		            &tok_err->location,
		            CARET_WORD,
		            "Function argument cannot be constant (this maybe shoule be possible "
		            "in future).");
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_err, scope_get(ctx)));
	}
	if (tokens_consume_if(ctx->tokens, SYM_ASSIGN)) {
		value = parse_hash_directive(ctx, HD_CALL_LOC, NULL, false);
		if (!value) value = parse_expr(ctx);
		if (!value) {
			struct token *tok_err = tokens_peek(ctx->tokens);
			builder_msg(MSG_ERR, ERR_EXPECTED_NAME, &tok_err->location, CARET_AFTER, "Expected .");
			return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_err, scope_get(ctx)));
		}
	} else if (!type) {
		builder_msg(
		    MSG_ERR, ERR_EXPECTED_TYPE, name->location, CARET_AFTER, "Expected argument type.");
		return_zone(
		    ast_create_node(ctx->ast_arena, AST_BAD, tokens_peek(ctx->tokens), scope_get(ctx)));
	}

	// Parse hash directives.
	u32 flags    = 0;
	u32 accepted = HD_COMPTIME | HD_MAYBE_UNUSED;
	while (true) {
		enum hash_directive_flags found = HD_NONE;
		parse_hash_directive(ctx, accepted, &found, false);
		if (!hash_directive_to_flags(found, &flags)) break;
		accepted &= ~found;
	}

	if (isflag(flags, FLAG_COMPTIME)) {
		// Currently compile time arguments cause changing of parent function generation to act like
		// a generated function, this is needed due to how compile time arguments replacement work.
		// Basically all comptime arguments are removed from a function signature in LLVM (kept for
		// analyze in MIR), its value (provided on call side) is used as compile time constant
		// in function body. That's why we need to generate each function specialization the same
		// way as polymorph functions does.
		set_parent_function_type_flavor(ctx, AST_TYPE_FN_FLAVOR_MIXED);
	}

	struct ast *arg          = ast_create_node(ctx->ast_arena, AST_DECL_ARG, tok_begin, scope_get(ctx));
	arg->data.decl_arg.value = value;
	arg->data.decl.type      = type;
	arg->data.decl.name      = name;
	arg->data.decl.flags     = flags;
	return_zone(arg);
}

struct ast *parse_decl_variant(struct context *ctx, struct ast *prev) {
	zone();
	struct token *tok_begin = tokens_peek(ctx->tokens);
	struct ast   *name      = parse_ident(ctx);
	if (!name) return_zone(NULL);
	struct ast *variant =
	    ast_create_node(ctx->ast_arena, AST_DECL_VARIANT, tok_begin, scope_get(ctx));
	variant->docs = pop_docs(ctx);

	struct token *tok_assign = tokens_consume_if(ctx->tokens, SYM_ASSIGN);
	if (tok_assign) {
		struct ast *expr = parse_expr(ctx);
		if (!expr) {
			struct token *tok_err = tokens_peek(ctx->tokens);
			builder_msg(MSG_ERR,
			            ERR_EXPECTED_NAME,
			            &tok_err->location,
			            CARET_AFTER,
			            "Expected enumerator variant value.");
			return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_err, scope_get(ctx)));
		}
		variant->data.decl_variant.value = expr;
	}
	variant->data.decl.name = name;
	return_zone(variant);
}

bool parse_semicolon(struct context *ctx) {
	return tokens_consume_if(ctx->tokens, SYM_SEMICOLON);
}

bool parse_semicolon_rq(struct context *ctx) {
	struct token *tok = tokens_consume_if(ctx->tokens, SYM_SEMICOLON);
	if (!tok && builder.errorc == 0) {
		tok = tokens_peek_prev(ctx->tokens);
		report_error(MISSING_SEMICOLON, tok, CARET_AFTER, "Expected semicolon ';'.");
		// consume_till(ctx->tokens, SYM_IDENT, SYM_RBLOCK); 2024-07-29 We don't want this everytime...
		return false;
	}
	return true;
}

bool hash_directive_to_flags(enum hash_directive_flags hd, u32 *out_flags) {
#define FLAG_CASE(_c, _f)     \
	case (_c):                \
		(*out_flags) |= (_f); \
		return true;          \
		(void)0

	switch (hd) {
		FLAG_CASE(HD_EXTERN, FLAG_EXTERN);
		FLAG_CASE(HD_INTRINSIC, FLAG_INTRINSIC);
		FLAG_CASE(HD_ENTRY, FLAG_ENTRY);
		FLAG_CASE(HD_BUILD_ENTRY, FLAG_BUILD_ENTRY);
		FLAG_CASE(HD_COMPILER, FLAG_COMPILER);
		FLAG_CASE(HD_INLINE, FLAG_INLINE);
		FLAG_CASE(HD_NO_INLINE, FLAG_NO_INLINE);
		FLAG_CASE(HD_NO_INIT, FLAG_NO_INIT);
		FLAG_CASE(HD_TEST_FN, FLAG_TEST_FN);
		FLAG_CASE(HD_EXPORT, FLAG_EXPORT);
		FLAG_CASE(HD_THREAD_LOCAL, FLAG_THREAD_LOCAL);
		FLAG_CASE(HD_FLAGS, FLAG_FLAGS);
		FLAG_CASE(HD_COMPTIME, FLAG_COMPTIME);
		FLAG_CASE(HD_MAYBE_UNUSED, FLAG_MAYBE_UNUSED);
		FLAG_CASE(HD_OBSOLETE, FLAG_OBSOLETE);
	default:
		break;
	}

	return false;
}

struct ast *parse_stmt_using(struct context *ctx) {
	zone();
	struct token *tok_begin = tokens_consume_if(ctx->tokens, SYM_USING);
	if (!tok_begin) return_zone(NULL);

	struct ast *stmt_using =
	    ast_create_node(ctx->ast_arena, AST_STMT_USING, tok_begin, scope_get(ctx));
	struct ast *expr = parse_expr(ctx);
	if (!expr) {
		struct token *tok_err = tokens_consume(ctx->tokens);
		report_error(EXPECTED_EXPR,
		             tok_err,
		             CARET_WORD,
		             "Expected scope or enumerator name after 'using' statement.");
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_err, scope_get(ctx)));
	}

	stmt_using->data.stmt_using.scope_expr = expr;
	return_zone(stmt_using);
}

struct ast *parse_stmt_return(struct context *ctx) {
	zone();
	struct token *tok_begin = tokens_consume_if(ctx->tokens, SYM_RETURN);
	if (!tok_begin) return_zone(NULL);
	struct ast *ret               = ast_create_node(ctx->ast_arena, AST_STMT_RETURN, tok_begin, scope_get(ctx));
	ret->data.stmt_return.fn_decl = decl_get(ctx);
	tok_begin                     = tokens_peek(ctx->tokens);

	struct ast *expr;
	bool        rq = false;
NEXT:
	expr = parse_expr(ctx);
	if (expr) {
		if (!ret->data.stmt_return.exprs) {
			ret->data.stmt_return.exprs = arena_alloc(ctx->sarr_arena);
		}
		sarrput(ret->data.stmt_return.exprs, expr);
		if (tokens_consume_if(ctx->tokens, SYM_COMMA)) {
			rq = true;
			goto NEXT;
		}
	} else if (rq) {
		struct token *tok_err = tokens_peek(ctx->tokens);
		report_error(EXPECTED_EXPR, tok_err, CARET_WORD, "Expected expression after comma ','.");
		consume_till(ctx->tokens, SYM_SEMICOLON, SYM_RBLOCK, SYM_IDENT);
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_err, scope_get(ctx)));
	}
	return_zone(ret);
}

struct ast *parse_stmt_if(struct context *ctx, bool is_static) {
	zone();

	struct token *tok_begin = tokens_consume_if(ctx->tokens, SYM_IF);
	if (!tok_begin) return_zone(NULL);

	const bool is_expression = ctx->is_inside_expression;

	struct ast *stmt_if                 = ast_create_node(ctx->ast_arena, AST_STMT_IF, tok_begin, scope_get(ctx));
	stmt_if->data.stmt_if.is_static     = is_static;
	stmt_if->data.stmt_if.is_expression = is_expression;
	stmt_if->data.stmt_if.test          = parse_expr(ctx);
	if (!stmt_if->data.stmt_if.test) {
		struct token *tok_err = tokens_consume(ctx->tokens);
		report_error(
		    EXPECTED_EXPR, tok_err, CARET_WORD, "Expected expression for the if statement.");
		tokens_consume_till(ctx->tokens, SYM_SEMICOLON);
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_err, scope_get(ctx)));
	}

	struct token *tok_then = tokens_consume_if(ctx->tokens, SYM_THEN);
	if (!tok_then && is_expression) {
		struct token *tok_err = tokens_peek(ctx->tokens);
		report_error(INVALID_EXPR, tok_err, CARET_WORD, "Expected 'then' keyword after ternary if statement expression.");
		tokens_consume_till(ctx->tokens, SYM_SEMICOLON);
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_err, scope_get(ctx)));
	}

	bool is_semicolon_required = !is_expression;

	//
	// Then branch
	//
	struct ast *true_branch = parse_block(ctx, SCOPE_LEXICAL);
	if (true_branch) {
		if (is_expression) {
			builder_msg(MSG_ERR, ERR_EXPECTED_EXPR, true_branch->location, CARET_WORD, "Blocks cannot be used in ternary if expressions.");
		}
		is_semicolon_required = false;
	}

	if (!true_branch && tok_then) {
		if (tokens_current_is(ctx->tokens, SYM_SEMICOLON) || tokens_current_is(ctx->tokens, SYM_ELSE)) {
			true_branch = NULL;
		} else if (is_expression) {
			true_branch = parse_single_block_stmt_or_expr(ctx, NULL);
		} else {
			struct scope *scope = scope_create(ctx->scope_thread_local, SCOPE_LEXICAL, scope_get(ctx), &tok_then->location);
			scope_push(ctx, scope);
			struct ast *block       = ast_create_node(ctx->ast_arena, AST_BLOCK, tok_then, scope_get(ctx));
			block->data.block.nodes = arena_alloc(ctx->sarr_arena);
			block_push(ctx, block);
			true_branch = parse_single_block_stmt_or_expr(ctx, NULL);
			sarrput(block->data.block.nodes, true_branch);
			block_pop(ctx);
			scope_pop(ctx);
			true_branch = block;
		}
	}

	if (!true_branch) {
		// Missing true branch (required).
		struct token *tok_err = tokens_consume(ctx->tokens);
		report_error(EXPECTED_STMT,
		             tok_err,
		             CARET_WORD,
		             "Expected statement, expression or block for the true result of the if test.");
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_err, scope_get(ctx)));
	}

	bassert(true_branch);
	stmt_if->data.stmt_if.true_stmt = true_branch;

	//
	// Else branch
	//
	struct ast *false_branch = NULL;
	if (tokens_consume_if(ctx->tokens, SYM_ELSE)) {
		false_branch = parse_stmt_if(ctx, is_static);
		if (!false_branch) false_branch = parse_block(ctx, SCOPE_LEXICAL);
		if (false_branch) {
			if (is_expression) {
				builder_msg(MSG_ERR, ERR_EXPECTED_EXPR, false_branch->location, CARET_WORD, "Blocks cannot be used in ternary if expressions.");
			}
			is_semicolon_required = false;
		}
		if (!false_branch) {
			if (tokens_current_is(ctx->tokens, SYM_SEMICOLON)) {
				false_branch = NULL;
			} else if (is_expression) {
				false_branch = parse_single_block_stmt_or_expr(ctx, NULL);
			} else {
				struct scope *scope = scope_create(ctx->scope_thread_local, SCOPE_LEXICAL, scope_get(ctx), &tok_then->location);
				scope_push(ctx, scope);
				struct ast *block       = ast_create_node(ctx->ast_arena, AST_BLOCK, tok_then, scope_get(ctx));
				block->data.block.nodes = arena_alloc(ctx->sarr_arena);
				block_push(ctx, block);
				false_branch = parse_single_block_stmt_or_expr(ctx, NULL);
				sarrput(block->data.block.nodes, false_branch);
				block_pop(ctx);
				scope_pop(ctx);
				false_branch = block;
			}
		}

		if (!false_branch) {
			// Missing false branch.
			struct token *tok_err = tokens_consume(ctx->tokens);
			report_error(EXPECTED_STMT,
			             tok_err,
			             CARET_WORD,
			             "Expected statement, expression or block for the false result of the if test.");
			return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_err, scope_get(ctx)));
		}

		bassert(false_branch);
	} else if (is_expression) {
		// @Note 2024-07-29: Expression require else to be present!
		struct token *tok_err = tokens_peek(ctx->tokens);
		report_error(EXPECTED_EXPR,
		             tok_err,
		             CARET_WORD,
		             "Expected else branch for ternary if expression. Ternary if expression evaluates into value which must be known for both expression test results.");
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_err, scope_get(ctx)));
	}

	stmt_if->data.stmt_if.false_stmt = false_branch;

	if (is_semicolon_required) {
		parse_semicolon_rq(ctx);
	}
	return_zone(stmt_if);
}

struct ast *parse_stmt_switch(struct context *ctx) {
	zone();
	struct token *tok_switch = tokens_consume_if(ctx->tokens, SYM_SWITCH);
	if (!tok_switch) return_zone(NULL);

	struct ast *expr = parse_expr(ctx);
	if (!expr) {
		struct token *tok_err = tokens_consume(ctx->tokens);
		report_error(
		    EXPECTED_EXPR, tok_err, CARET_WORD, "Expected expression for the switch statement.");
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_err, scope_get(ctx)));
	}

	struct token *tok = tokens_consume_if(ctx->tokens, SYM_LBLOCK);
	if (!tok) {
		struct token *tok_err = tokens_peek(ctx->tokens);
		report_error(EXPECTED_BODY, tok_err, CARET_WORD, "Expected switch body block.");
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_err, scope_get(ctx)));
	}

	ast_nodes_t *cases        = arena_alloc(ctx->sarr_arena);
	struct ast  *stmt_case    = NULL;
	struct ast  *default_case = NULL;
NEXT:
	stmt_case = parse_stmt_case(ctx);
	if (AST_IS_OK(stmt_case)) {
		if (stmt_case->data.stmt_case.is_default) {
			if (default_case) {
				builder_msg(MSG_ERR,
				            ERR_INVALID_SWITCH_CASE,
				            stmt_case->location,
				            CARET_WORD,
				            "Switch statement cannot have more than one default cases.");

				builder_msg(
				    MSG_ERR_NOTE, 0, default_case->location, CARET_WORD, "Previous found here.");
			} else {
				default_case = stmt_case;
			}
		}

		sarrput(cases, stmt_case);
		if (tokens_current_is_not(ctx->tokens, SYM_RBLOCK)) goto NEXT;
	}

	tok = tokens_consume_if(ctx->tokens, SYM_RBLOCK);
	if (!tok) {
		struct token *tok_err = tokens_peek(ctx->tokens);
		report_error(EXPECTED_BODY, tok_err, CARET_WORD, "Expected end of switch body block.");
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_err, scope_get(ctx)));
	}

	struct ast *stmt_switch =
	    ast_create_node(ctx->ast_arena, AST_STMT_SWITCH, tok_switch, scope_get(ctx));

	stmt_switch->data.stmt_switch.expr  = expr;
	stmt_switch->data.stmt_switch.cases = cases;
	return_zone(stmt_switch);
}

struct ast *parse_stmt_case(struct context *ctx) {
	zone();
	ast_nodes_t *exprs = NULL;
	struct ast  *block = NULL;
	struct ast  *expr  = NULL;
	bool         rq    = false;

	if (tokens_current_is(ctx->tokens, SYM_RBLOCK)) return_zone(NULL);

	struct token *tok_case = tokens_consume_if(ctx->tokens, SYM_DEFAULT);
	if (tok_case) goto SKIP_EXPRS;

	tok_case = tokens_peek(ctx->tokens);
	exprs    = arena_alloc(ctx->sarr_arena);
NEXT:
	expr = parse_expr(ctx);
	if (expr) {
		sarrput(exprs, expr);

		if (tokens_consume_if(ctx->tokens, SYM_COMMA)) {
			rq = true;
			goto NEXT;
		}
	} else if (rq) {
		struct token *tok_err = tokens_peek(ctx->tokens);
		report_error(EXPECTED_NAME, tok_err, CARET_WORD, "Expected expression after comma.");
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_err, scope_get(ctx)));
	}

SKIP_EXPRS:
	block = parse_block(ctx, SCOPE_LEXICAL);
	if (!block && !parse_semicolon_rq(ctx)) {
		struct token *tok_err = tokens_peek(ctx->tokens);
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_err, scope_get(ctx)));
	} else {
		parse_semicolon(ctx);
	}

	struct ast *stmt_case =
	    ast_create_node(ctx->ast_arena, AST_STMT_CASE, tok_case, scope_get(ctx));
	stmt_case->data.stmt_case.exprs      = exprs;
	stmt_case->data.stmt_case.is_default = !exprs;
	stmt_case->data.stmt_case.block      = block;

	return_zone(stmt_case);
}

static enum tokens_lookahead_state cmp_stmt_loop(struct token *curr) {
	if (token_is(curr, SYM_SEMICOLON))
		return TOK_LOOK_HIT;
	else if (token_is(curr, SYM_LBLOCK))
		return TOK_LOOK_TERMINAL;

	return TOK_LOOK_CONTINUE;
}

struct ast *parse_stmt_loop(struct context *ctx) {
	zone();
	struct token *tok_begin = tokens_consume_if(ctx->tokens, SYM_LOOP);
	if (!tok_begin) return_zone(NULL);

	// Loop statement is immediately followed by block; this should act like while (true) {} in C.
	const bool while_true = tokens_current_is(ctx->tokens, SYM_LBLOCK);

	struct ast *loop         = ast_create_node(ctx->ast_arena, AST_STMT_LOOP, tok_begin, scope_get(ctx));
	const bool  prev_in_loop = ctx->is_inside_loop;
	ctx->is_inside_loop      = true;

	struct scope *scope = scope_create(ctx->scope_thread_local, SCOPE_LEXICAL, scope_get(ctx), &tok_begin->location);

	scope_push(ctx, scope);

	if (!while_true) {
		if (tokens_lookahead(ctx->tokens, cmp_stmt_loop)) {
			// for loop construct loop [init]; [condition]; [increment] {}
			loop->data.stmt_loop.init = parse_decl(ctx);
			if (!parse_semicolon_rq(ctx)) {
				bassert(false);
			}

			loop->data.stmt_loop.condition = parse_expr(ctx);
			if (!parse_semicolon_rq(ctx)) {
				bassert(false);
			}

			loop->data.stmt_loop.increment = parse_expr(ctx);
		} else {
			// while construct with optional condition
			loop->data.stmt_loop.condition = parse_expr(ctx);
		}
	}

	// block
	loop->data.stmt_loop.block = parse_block(ctx, SCOPE_NONE);
	if (!loop->data.stmt_loop.block) {
		struct token *tok_err = tokens_peek(ctx->tokens);
		report_error(EXPECTED_BODY, tok_err, CARET_WORD, "Expected loop body block.");
		ctx->is_inside_loop = prev_in_loop;
		scope_pop(ctx);
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_err, scope_get(ctx)));
	}

	ctx->is_inside_loop = prev_in_loop;
	scope_pop(ctx);
	return_zone(loop);
}

struct ast *parse_stmt_break(struct context *ctx) {
	zone();
	struct token *tok = tokens_consume_if(ctx->tokens, SYM_BREAK);
	if (!tok) return_zone(NULL);

	if (!ctx->is_inside_loop) {
		report_error(BREAK_OUTSIDE_LOOP, tok, CARET_WORD, "Break statement outside a loop.");
	}
	return_zone(ast_create_node(ctx->ast_arena, AST_STMT_BREAK, tok, scope_get(ctx)));
}

struct ast *parse_stmt_continue(struct context *ctx) {
	zone();
	struct token *tok = tokens_consume_if(ctx->tokens, SYM_CONTINUE);
	if (!tok) return_zone(NULL);

	if (!ctx->is_inside_loop) {
		report_error(CONTINUE_OUTSIDE_LOOP, tok, CARET_WORD, "Continue statement outside a loop.");
	}

	return_zone(ast_create_node(ctx->ast_arena, AST_STMT_CONTINUE, tok, scope_get(ctx)));
}

struct ast *parse_stmt_defer(struct context *ctx) {
	zone();
	struct token *tok = tokens_consume_if(ctx->tokens, SYM_DEFER);
	if (!tok) return_zone(NULL);

	struct ast *expr = NULL;
	expr             = parse_expr(ctx);

	if (!expr) {
		report_error(
		    EXPECTED_EXPR, tok, CARET_WORD, "Expected expression after 'defer' statement.");
		struct token *tok_err = tokens_peek(ctx->tokens);
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_err, scope_get(ctx)));
	}

	struct ast *defer           = ast_create_node(ctx->ast_arena, AST_STMT_DEFER, tok, scope_get(ctx));
	defer->data.stmt_defer.expr = expr;

	return_zone(defer);
}

struct ast *parse_expr(struct context *ctx) {
	return _parse_expr(ctx, 0);
}

struct ast *_parse_expr(struct context *ctx, s32 p) {
	zone();
	const bool prev_is_inside_expression = ctx->is_inside_expression;
	ctx->is_inside_expression            = true;
	struct ast *lhs                      = parse_expr_atom(ctx);
	struct ast *tmp                      = NULL;
	do {
		tmp = parse_expr_call(ctx, lhs);
		if (!tmp) tmp = parse_expr_elem(ctx, lhs);
		if (!tmp) tmp = parse_expr_compound(ctx, lhs);
		if (!tmp) tmp = parse_ref_nested(ctx, lhs);
		lhs = tmp ? tmp : lhs;
	} while (tmp);

	while (token_is_binop(tokens_peek(ctx->tokens)) &&
	       token_prec(tokens_peek(ctx->tokens)).priority >= p) {
		struct token *op  = tokens_consume(ctx->tokens);
		const s32     q   = token_prec(op).associativity == TOKEN_ASSOC_LEFT ? token_prec(op).priority + 1
		                                                                     : token_prec(op).priority;
		struct ast   *rhs = _parse_expr(ctx, q);
		if (!lhs || !rhs) {
			report_error(INVALID_EXPR, op, CARET_WORD, "Invalid binary operation.");
		}
		lhs = parse_expr_binary(ctx, lhs, rhs, op);
	}
	ctx->is_inside_expression = prev_is_inside_expression;
	return_zone(lhs);
}

struct ast *parse_expr_primary(struct context *ctx) {
	struct ast *expr = NULL;
	switch (tokens_peek_sym(ctx->tokens)) {
	case SYM_LPAREN:
		expr = parse_expr_nested(ctx);
		break;
	case SYM_IDENT:
		expr = parse_expr_ref(ctx);
		break;
	case SYM_NULL:
		expr = parse_expr_null(ctx);
		break;
	case SYM_HASH:
		expr = parse_hash_directive(ctx, HD_FILE | HD_LINE | HD_IMPORT, NULL, true);
		break;
	case SYM_FN:
		if ((expr = parse_expr_lit_fn(ctx))) break;
		expr = parse_expr_lit_fn_group(ctx);
		break;
	case SYM_IF:
		expr = parse_stmt_if(ctx, false);
		break;
	default:
		break;
	}
	if (expr) return expr;
	if ((expr = parse_expr_lit(ctx))) return expr;
	if ((expr = parse_expr_type(ctx))) return expr;
	return NULL;
}

struct ast *parse_expr_unary(struct context *ctx) {
	zone();
	struct token *op = tokens_peek(ctx->tokens);
	if (!token_is_unary(op)) return_zone(NULL);

	tokens_consume(ctx->tokens);
	struct ast *unary           = ast_create_node(ctx->ast_arena, AST_EXPR_UNARY, op, scope_get(ctx));
	unary->data.expr_unary.next = _parse_expr(ctx, token_prec(op).priority);
	unary->data.expr_unary.kind = sym_to_unop_kind(op->sym);

	if (unary->data.expr_unary.next == NULL) {
		struct token *err_tok = tokens_peek(ctx->tokens);
		report_error(
		    EXPECTED_EXPR, err_tok, CARET_WORD, "Expected expression after unary operator.");
		tokens_consume_till(ctx->tokens, SYM_SEMICOLON);
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, op, scope_get(ctx)));
	}

	if (unary->data.expr_unary.next->kind == AST_BAD) return_zone(unary->data.expr_unary.next);

	return_zone(unary);
}

struct ast *parse_expr_atom(struct context *ctx) {
	struct ast *expr = NULL;
	switch (tokens_peek_sym(ctx->tokens)) {
	case SYM_AT:
		expr = parse_expr_deref(ctx);
		break;
	case SYM_AND:
		expr = parse_expr_addrof(ctx);
		break;
	case SYM_CAST:
		expr = parse_expr_cast(ctx);
		break;
	case SYM_CAST_AUTO:
		expr = parse_expr_cast_auto(ctx);
		break;
	case SYM_TESTCASES:
		expr = parse_expr_test_cases(ctx);
		break;
	default:
		break;
	}
	if (expr) return expr;
	if ((expr = parse_expr_primary(ctx))) return expr;
	if ((expr = parse_expr_unary(ctx))) return expr;
	return NULL;
}

struct ast *
parse_expr_binary(struct context *ctx, struct ast *lhs, struct ast *rhs, struct token *op) {
	zone();
	if (!token_is_binop(op)) return_zone(NULL);

	struct ast *binop           = ast_create_node(ctx->ast_arena, AST_EXPR_BINOP, op, scope_get(ctx));
	binop->data.expr_binop.kind = sym_to_binop_kind(op->sym);
	binop->data.expr_binop.lhs  = lhs;
	binop->data.expr_binop.rhs  = rhs;
	return_zone(binop);
}

struct ast *parse_expr_addrof(struct context *ctx) {
	zone();
	struct token *tok = tokens_consume_if(ctx->tokens, SYM_AND);
	if (!tok) return_zone(NULL);

	struct ast *addrof            = ast_create_node(ctx->ast_arena, AST_EXPR_ADDROF, tok, scope_get(ctx));
	addrof->data.expr_addrof.next = _parse_expr(ctx, token_prec(tok).priority);

	if (addrof->data.expr_addrof.next == NULL) {
		struct token *err_tok = tokens_peek(ctx->tokens);
		report_error(EXPECTED_EXPR, err_tok, CARET_WORD, "Expected expression after '&' operator.");
		tokens_consume_till(ctx->tokens, SYM_SEMICOLON);
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok, scope_get(ctx)));
	}

	if (addrof->data.expr_addrof.next->kind == AST_BAD) return_zone(addrof->data.expr_addrof.next);
	return_zone(addrof);
}

struct ast *parse_expr_deref(struct context *ctx) {
	zone();
	struct token *tok = tokens_consume_if(ctx->tokens, SYM_AT);
	if (!tok) return_zone(NULL);

	struct ast *deref           = ast_create_node(ctx->ast_arena, AST_EXPR_DEREF, tok, scope_get(ctx));
	deref->data.expr_deref.next = _parse_expr(ctx, token_prec(tok).priority);

	if (deref->data.expr_deref.next == NULL) {
		struct token *err_tok = tokens_peek(ctx->tokens);
		report_error(EXPECTED_EXPR,
		             err_tok,
		             CARET_WORD,
		             "Expected expression after '@' pointer dereference operator.");
		tokens_consume_till(ctx->tokens, SYM_SEMICOLON);
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok, scope_get(ctx)));
	}

	if (deref->data.expr_deref.next->kind == AST_BAD) return_zone(deref->data.expr_deref.next);
	return_zone(deref);
}

struct ast *parse_expr_lit(struct context *ctx) {
	zone();
	struct token *tok = tokens_peek(ctx->tokens);
	struct ast   *lit = NULL;

	switch (tok->sym) {
	case SYM_NUM:
		lit                        = ast_create_node(ctx->ast_arena, AST_EXPR_LIT_INT, tok, scope_get(ctx));
		lit->data.expr_integer.val = get_token_value(ctx, tok).number;
		break;

	case SYM_CHAR:
		lit                          = ast_create_node(ctx->ast_arena, AST_EXPR_LIT_CHAR, tok, scope_get(ctx));
		lit->data.expr_character.val = (u8)get_token_value(ctx, tok).character;

		break;

	case SYM_TRUE:
		lit                        = ast_create_node(ctx->ast_arena, AST_EXPR_LIT_BOOL, tok, scope_get(ctx));
		lit->data.expr_boolean.val = true;
		break;

	case SYM_FALSE:
		lit                        = ast_create_node(ctx->ast_arena, AST_EXPR_LIT_BOOL, tok, scope_get(ctx));
		lit->data.expr_boolean.val = false;
		break;

	case SYM_DOUBLE:
		lit                       = ast_create_node(ctx->ast_arena, AST_EXPR_LIT_DOUBLE, tok, scope_get(ctx));
		lit->data.expr_double.val = get_token_value(ctx, tok).double_number;
		break;

	case SYM_FLOAT:
		lit                      = ast_create_node(ctx->ast_arena, AST_EXPR_LIT_FLOAT, tok, scope_get(ctx));
		lit->data.expr_float.val = (f32)get_token_value(ctx, tok).double_number;
		break;

	case SYM_STRING: {
		// There is special case for string literals, those can be split into multiple lines and we
		// should handle such situation here, so some pre-scan is needed.
		lit                    = ast_create_node(ctx->ast_arena, AST_EXPR_LIT_STRING, tok, scope_get(ctx));
		struct token *tok_next = tokens_peek_2nd(ctx->tokens);
		if (tok_next->sym == SYM_STRING) {
			str_buf_t tmp = get_tmp_str();

			while ((tok = tokens_consume_if(ctx->tokens, SYM_STRING))) {
				const str_t str = get_token_value(ctx, tok).str;
				str_buf_append(&tmp, str);
			}
			const str_t dup = scdup2(ctx->string_cache, tmp);
			put_tmp_str(tmp);

			lit->data.expr_string.val = dup;
		} else {
			tokens_consume(ctx->tokens);
			const str_t str           = get_token_value(ctx, tok).str;
			lit->data.expr_string.val = str;
		}
		// Directly return, all tokens were consumed.
		return_zone(lit);
	}

	default:
		return_zone(NULL);
	}

	tokens_consume(ctx->tokens);
	return_zone(lit);
}

struct ast *parse_expr_lit_fn(struct context *ctx) {
	zone();
	if (!tokens_is_seq(ctx->tokens, 2, SYM_FN, SYM_LPAREN)) return_zone(NULL);
	struct token *tok_fn = tokens_peek(ctx->tokens);
	struct ast   *fn     = ast_create_node(ctx->ast_arena, AST_EXPR_LIT_FN, tok_fn, scope_get(ctx));

	const bool prev_is_inside_expression = ctx->is_inside_expression;
	ctx->is_inside_expression            = false;

	// Create function scope for function signature.
	struct scope *scope = scope_create(ctx->scope_thread_local, SCOPE_FN, scope_get(ctx), &tok_fn->location);
	scope_push(ctx, scope);

	struct ast *type = parse_type_fn(ctx, /* named_args */ true, /* create_scope */ false);
	bassert(type);
	fn->data.expr_fn.type = type;
	// parse flags
	struct ast *curr_decl = decl_get(ctx);
	if (curr_decl && curr_decl->kind == AST_DECL_ENTITY) {
		u32 accepted = HD_EXTERN | HD_NO_INLINE | HD_INLINE | HD_COMPILER | HD_ENTRY |
		               HD_BUILD_ENTRY | HD_INTRINSIC | HD_TEST_FN | HD_EXPORT | HD_COMPTIME |
		               HD_MAYBE_UNUSED | HD_OBSOLETE | HD_ENABLE_IF;
		u32 flags = 0;
		while (true) {
			enum hash_directive_flags found        = HD_NONE;
			struct ast               *hd_extension = parse_hash_directive(ctx, accepted, &found, false);
			if (found == HD_ENABLE_IF) {
				bassert(hd_extension);
				fn->data.expr_fn.enable_if = hd_extension;
			} else if (hash_directive_to_flags(found, &flags)) {
				if ((found == HD_EXTERN || found == HD_INTRINSIC || found == HD_EXPORT) &&
				    hd_extension) {
					// Use extern flag extension on function declaration.
					bassert(hd_extension->kind == AST_IDENT &&
					        "Expected ident as #extern extension.");
					bassert(curr_decl->data.decl_entity.explicit_linkage_name == NULL);
					curr_decl->data.decl_entity.explicit_linkage_name = hd_extension;
				} else if (found == HD_OBSOLETE && hd_extension) {
					bassert(hd_extension->kind == AST_EXPR_LIT_STRING &&
					        "The obsolete hash directive extension is supposed to be a string "
					        "literal.");
					fn->data.expr_fn.obsolete_warning_message = hd_extension;
				}
			} else {
				break;
			}

			accepted &= ~found;
		}
		curr_decl->data.decl.flags |= flags;
	}

	// parse block (block is optional function body can be external)
	fn->data.expr_fn.block = parse_block(ctx, SCOPE_FN_BODY);

	ctx->is_inside_expression = prev_is_inside_expression;

	scope_pop(ctx);
	return_zone(fn);
}

struct ast *parse_expr_lit_fn_group(struct context *ctx) {
	zone();
	if (!tokens_is_seq(ctx->tokens, 2, SYM_FN, SYM_LBLOCK)) return_zone(NULL);
	struct token *tok_group = tokens_consume(ctx->tokens); // eat fn
	struct token *tok_begin = tokens_consume(ctx->tokens); // eat {
	struct ast   *group     = ast_create_node(ctx->ast_arena, AST_EXPR_LIT_FN_GROUP, tok_group, scope_get(ctx));

	struct scope *scope = scope_create(ctx->scope_thread_local, SCOPE_FN_GROUP, scope_get(ctx), &tok_begin->location);
	scope_push(ctx, scope);

	ast_nodes_t *variants              = arena_alloc(ctx->sarr_arena);
	group->data.expr_fn_group.variants = variants;
	struct ast *tmp;
NEXT:
	if ((tmp = parse_expr(ctx))) {
		sarrput(variants, tmp);
		parse_semicolon_rq(ctx);
		goto NEXT;
	}

	scope_pop(ctx);

	struct token *tok = tokens_consume_if(ctx->tokens, SYM_RBLOCK);
	if (!tok) {
		tok = tokens_peek_prev(ctx->tokens);
		report_error(EXPECTED_BODY_END, tok, CARET_AFTER, "Expected end of block '}'.");
		report_note(tok_begin, CARET_WORD, "Block starting here.");
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_begin, scope_get(ctx)));
	}
	return_zone(group);
}

struct ast *parse_expr_nested(struct context *ctx) {
	zone();
	struct ast   *expr      = NULL;
	struct token *tok_begin = tokens_consume_if(ctx->tokens, SYM_LPAREN);
	if (!tok_begin) return_zone(NULL);

	expr = parse_expr(ctx);
	if (expr == NULL) {
		report_error(EXPECTED_EXPR, tok_begin, CARET_WORD, "Expected expression.");
	}

	// eat )
	struct token *tok_end = tokens_consume_if(ctx->tokens, SYM_RPAREN);
	if (!tok_end) {
		struct token *tok_err = tokens_peek(ctx->tokens);
		report_error(
		    MISSING_BRACKET, tok_err, CARET_WORD, "Unterminated sub-expression, missing ')'.");
		report_note(tok_begin, CARET_WORD, "starting here");
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_begin, scope_get(ctx)));
	}

	return_zone(expr);
}

struct ast *parse_expr_elem(struct context *ctx, struct ast *prev) {
	zone();
	if (!prev) return_zone(NULL);
	struct token *tok_elem = tokens_consume_if(ctx->tokens, SYM_LBRACKET);
	if (!tok_elem) return_zone(NULL);

	struct ast *elem           = ast_create_node(ctx->ast_arena, AST_EXPR_ELEM, tok_elem, scope_get(ctx));
	elem->data.expr_elem.index = parse_expr(ctx);
	elem->data.expr_elem.next  = prev;

	if (!elem->data.expr_elem.index) {
		report_error(EXPECTED_EXPR, tok_elem, CARET_WORD, "Expected array index expression.");
	}

	struct token *tok = tokens_consume(ctx->tokens);
	if (tok->sym != SYM_RBRACKET) {
		report_error(MISSING_BRACKET, tok, CARET_WORD, "Missing bracket ']'.");
	}

	return_zone(elem);
}

struct ast *parse_ident_group(struct context *ctx) {
	zone();
	struct ast *root = NULL;
	struct ast *prev = NULL;
	bool        rq   = false;
	struct ast *ident;
NEXT:
	ident = parse_ident(ctx);
	if (ident) {
		if (prev) prev->data.ident.next = ident;
		if (!root) root = ident;
		prev = ident;
		if (tokens_consume_if(ctx->tokens, SYM_COMMA)) {
			rq = true;
			goto NEXT;
		}
	} else if (rq) {
		struct token *tok_err = tokens_peek(ctx->tokens);
		report_error(EXPECTED_NAME, tok_err, CARET_WORD, "Expected name after comma ','.");
		consume_till(ctx->tokens, SYM_COLON, SYM_SEMICOLON, SYM_IDENT);
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_err, scope_get(ctx)));
	}
	return_zone(root);
}

struct ast *parse_type_ptr(struct context *ctx) {
	zone();
	struct token *tok_begin = tokens_consume_if(ctx->tokens, SYM_ASTERISK);
	if (!tok_begin) return_zone(NULL);

	struct ast *ptr      = ast_create_node(ctx->ast_arena, AST_TYPE_PTR, tok_begin, scope_get(ctx));
	struct ast *sub_type = parse_type(ctx);
	if (!sub_type) {
		struct token *tok_err = tokens_peek(ctx->tokens);
		report_error(EXPECTED_TYPE,
		             tok_err,
		             CARET_WORD,
		             "Expected type after '*' pointer type declaration.");
		consume_till(ctx->tokens, SYM_COLON, SYM_SEMICOLON, SYM_IDENT);
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_err, scope_get(ctx)));
	}
	ptr->data.type_ptr.type = sub_type;
	return_zone(ptr);
}

struct ast *parse_type_vargs(struct context *ctx) {
	zone();
	struct token *tok_begin = tokens_consume_if(ctx->tokens, SYM_VARGS);
	if (!tok_begin) return_zone(NULL);

	struct ast *ptr         = ast_create_node(ctx->ast_arena, AST_TYPE_VARGS, tok_begin, scope_get(ctx));
	ptr->data.type_ptr.type = parse_type(ctx);
	return_zone(ptr);
}

struct ast *parse_type_enum(struct context *ctx) {
	zone();
	struct token *tok_enum = tokens_consume_if(ctx->tokens, SYM_ENUM);
	if (!tok_enum) return_zone(NULL);

	struct ast *enm             = ast_create_node(ctx->ast_arena, AST_TYPE_ENUM, tok_enum, scope_get(ctx));
	enm->data.type_enm.variants = arena_alloc(ctx->sarr_arena);
	enm->data.type_enm.type     = parse_type(ctx);

	// parse flags
	{
		u32 accepted = HD_COMPILER | HD_FLAGS;
		u32 flags    = 0;
		while (true) {
			enum hash_directive_flags found = HD_NONE;
			parse_hash_directive(ctx, accepted, &found, false);
			if (!hash_directive_to_flags(found, &flags)) break;
			accepted &= ~found;
		}
		struct ast *curr_decl = decl_get(ctx);
		if (curr_decl && curr_decl->kind == AST_DECL_ENTITY) {
			curr_decl->data.decl.flags |= flags;
		}
		enm->data.type_enm.is_flags = isflag(flags, FLAG_FLAGS);
	}

	struct token *tok = tokens_consume(ctx->tokens);
	if (token_is_not(tok, SYM_LBLOCK)) {
		report_error(MISSING_BRACKET, tok, CARET_WORD, "Expected enum variant list.");
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok, scope_get(ctx)));
	}

	struct scope *scope = scope_create(ctx->scope_thread_local, SCOPE_TYPE_ENUM, scope_get(ctx), &tok->location);
	scope_reserve(scope, 256);

	enm->data.type_enm.scope = scope;
	scope_push(ctx, scope);

	// parse enum varinats
	bool        rq = false;
	struct ast *tmp;
	struct ast *prev_tmp = NULL;

NEXT:
	if (parse_docs(ctx)) goto NEXT;
	tmp = parse_decl_variant(ctx, prev_tmp);
	if (tmp) {
		prev_tmp = tmp;
		sarrput(enm->data.type_enm.variants, tmp);

		if (tokens_consume_if(ctx->tokens, SYM_SEMICOLON)) {
			rq = true;
			goto NEXT;
		}
	} else if (rq) {
		struct token *tok_err = tokens_peek(ctx->tokens);
		if (tokens_peek_2nd(ctx->tokens)->sym == SYM_RBLOCK) {
			report_error(EXPECTED_NAME, tok_err, CARET_WORD, "Expected variant after semicolon.");
			scope_pop(ctx);
			return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok, scope_get(ctx)));
		}
	}

	tok = tokens_consume(ctx->tokens);
	if (tok->sym != SYM_RBLOCK) {
		report_error(MISSING_BRACKET,
		             tok,
		             CARET_WORD,
		             "Expected end of variant list '}' or another variant separated by semicolon.");
		scope_pop(ctx);
		tokens_consume_till(ctx->tokens, SYM_SEMICOLON);
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok, scope_get(ctx)));
	}

	scope_pop(ctx);
	return_zone(enm);
}

struct ast *parse_ref(struct context *ctx) {
	zone();
	struct token *tok   = tokens_peek(ctx->tokens);
	struct ast   *ident = parse_ident(ctx);
	if (!ident) return_zone(NULL);
	struct ast *lhs     = ast_create_node(ctx->ast_arena, AST_REF, tok, scope_get(ctx));
	lhs->data.ref.ident = ident;
	struct ast *tmp     = NULL;
	do {
		tmp = parse_ref_nested(ctx, lhs);
		lhs = tmp ? tmp : lhs;
	} while (tmp);
	return_zone(lhs);
}

struct ast *parse_ref_nested(struct context *ctx, struct ast *prev) {
	zone();
	if (!prev) return_zone(NULL);
	// @Hack: This is a little hack for now, because we have .{ combination reserved for compound
	// initializers.
	if (tokens_peek_2nd(ctx->tokens)->sym == SYM_LBLOCK) return_zone(NULL);
	struct token *tok = tokens_consume_if(ctx->tokens, SYM_DOT);
	if (!tok) return_zone(NULL);

	struct ast *ident = parse_ident(ctx);
	if (!ident) {
		struct token *tok_err = tokens_peek(ctx->tokens);
		report_error(EXPECTED_NAME, tok_err, CARET_WORD, "Expected name.");
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok, scope_get(ctx)));
	}

	struct ast *ref     = ast_create_node(ctx->ast_arena, AST_REF, tok, scope_get(ctx));
	ref->data.ref.ident = ident;
	ref->data.ref.next  = prev;
	return_zone(ref);
}

struct ast *parse_type_polymorph(struct context *ctx) {
	zone();
	struct token *tok_begin = tokens_consume_if(ctx->tokens, SYM_QUESTION);
	if (!tok_begin) return_zone(NULL);

	if (!arrlenu(ctx->fn_type_stack)) {
		report_error(INVALID_ARG_TYPE,
		             tok_begin,
		             CARET_WORD,
		             "Polymorph type can be specified only in function argument list.");
		tokens_consume_till(ctx->tokens, SYM_SEMICOLON);
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_begin, scope_get(ctx)));
	}
	set_parent_function_type_flavor(ctx, AST_TYPE_FN_FLAVOR_POLYMORPH);
	struct ast *ident = parse_ident(ctx);
	if (!ident) {
		struct token *tok_err = tokens_peek(ctx->tokens);
		report_error(EXPECTED_NAME, tok_err, CARET_WORD, "Expected name of polymorph type.");
		tokens_consume_till(ctx->tokens, SYM_SEMICOLON);
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_begin, scope_get(ctx)));
	}
	struct ast *poly           = ast_create_node(ctx->ast_arena, AST_TYPE_POLY, tok_begin, scope_get(ctx));
	poly->data.type_poly.ident = ident;
	return_zone(poly);
}

struct ast *parse_type_arr(struct context *ctx) {
	zone();
	struct token *tok_begin = tokens_consume_if(ctx->tokens, SYM_LBRACKET);
	if (!tok_begin) return_zone(NULL);

	struct ast *arr        = ast_create_node(ctx->ast_arena, AST_TYPE_ARR, tok_begin, scope_get(ctx));
	struct ast *len        = parse_expr(ctx);
	arr->data.type_arr.len = len;

	if (!len) {
		struct token *tok_err = tokens_peek(ctx->tokens);
		report_error(EXPECTED_EXPR, tok_err, CARET_WORD, "Expected array size expression.");
		tokens_consume_till(ctx->tokens, SYM_SEMICOLON);
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_begin, scope_get(ctx)));
	}

	if (len->kind == AST_REF) {
		bassert(len->data.ref.ident);
		arr->data.type_arr.is_len_inferred_from_compound = is_ignored_id(&len->data.ref.ident->data.ident.id);
	}

	struct token *tok_end = tokens_consume_if(ctx->tokens, SYM_RBRACKET);
	if (!tok_end) {
		struct token *tok_err = tokens_peek(ctx->tokens);
		report_error(MISSING_BRACKET,
		             tok_err,
		             CARET_WORD,
		             "Expected closing ']' after array size expression.");
		tokens_consume_till(ctx->tokens, SYM_SEMICOLON);

		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_begin, scope_get(ctx)));
	}

	arr->data.type_arr.elem_type = parse_type(ctx);
	if (!arr->data.type_arr.elem_type) {
		struct token *tok_err = tokens_peek(ctx->tokens);
		report_error(INVALID_TYPE, tok_err, CARET_WORD, "Expected array element type.");
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_begin, scope_get(ctx)));
	}

	return_zone(arr);
}

struct ast *parse_type_slice(struct context *ctx) {
	zone();
	if (tokens_peek(ctx->tokens)->sym != SYM_LBRACKET) return_zone(NULL);
	if (tokens_peek_2nd(ctx->tokens)->sym != SYM_RBRACKET) return_zone(NULL);

	// eat []
	struct token *tok_begin = tokens_consume(ctx->tokens);
	tok_begin               = tokens_consume(ctx->tokens);

	struct ast *slice = ast_create_node(ctx->ast_arena, AST_TYPE_SLICE, tok_begin, scope_get(ctx));

	slice->data.type_slice.elem_type = parse_type(ctx);

	if (!slice->data.type_slice.elem_type) {
		report_error(INVALID_TYPE, tok_begin, CARET_AFTER, "Expected slice element type.");
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_begin, scope_get(ctx)));
	}

	return_zone(slice);
}

struct ast *parse_type_dynarr(struct context *ctx) {
	zone();
	if (tokens_peek(ctx->tokens)->sym != SYM_LBRACKET) return_zone(NULL);
	if (tokens_peek_2nd(ctx->tokens)->sym != SYM_DYNARR) return_zone(NULL);

	// eat [..
	struct token *tok_begin = tokens_consume(ctx->tokens);
	tokens_consume(ctx->tokens);

	struct token *tok_end = tokens_consume_if(ctx->tokens, SYM_RBRACKET);
	if (!tok_end) {
		struct token *tok_err = tokens_peek(ctx->tokens);
		report_error(MISSING_BRACKET,
		             tok_err,
		             CARET_WORD,
		             "Expected closing ']' after dynamic array signature.");
		tokens_consume_till(ctx->tokens, SYM_SEMICOLON);

		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_begin, scope_get(ctx)));
	}

	struct ast *slice                 = ast_create_node(ctx->ast_arena, AST_TYPE_DYNARR, tok_begin, scope_get(ctx));
	slice->data.type_dynarr.elem_type = parse_type(ctx);

	if (!slice->data.type_dynarr.elem_type) {
		report_error(INVALID_TYPE, tok_end, CARET_AFTER, "Expected dynamic array element type.");
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_begin, scope_get(ctx)));
	}

	return_zone(slice);
}

struct ast *parse_type(struct context *ctx) {
	struct ast *type = NULL;

	type = parse_type_ptr(ctx);
	// keep order
	if (!type) type = parse_type_fn_group(ctx);
	if (!type) type = parse_type_fn(ctx, /* named_args */ false, /* create_scope */ true);
	// keep order

	if (!type) type = parse_type_polymorph(ctx);
	if (!type) type = parse_type_struct(ctx);
	if (!type) type = parse_type_enum(ctx);
	if (!type) type = parse_type_vargs(ctx);

	// Keep order!!!
	if (!type) type = parse_type_slice(ctx);
	if (!type) type = parse_type_dynarr(ctx);
	if (!type) type = parse_type_arr(ctx);
	// Keep order!!!

	if (!type) type = parse_expr_deref(ctx);
	if (!type) {
		struct ast *ref = parse_ref(ctx);
		if (ref) type = parse_expr_call(ctx, ref);
		if (!type) type = ref;
	}

	return type;
}

struct ast *parse_type_fn_return(struct context *ctx) {
	zone();
	if (tokens_current_is(ctx->tokens, SYM_LPAREN)) {
		// multiple return type ( T1, T2 )
		// eat (
		struct token *tok_begin = tokens_consume(ctx->tokens);
		struct scope *scope     = scope_create(ctx->scope_thread_local, SCOPE_TYPE_STRUCT, scope_get(ctx), &tok_begin->location);
		scope_push(ctx, scope);

		struct ast *type_struct =
		    ast_create_node(ctx->ast_arena, AST_TYPE_STRUCT, tok_begin, scope_get(ctx));
		type_struct->data.type_strct.scope                   = scope;
		type_struct->data.type_strct.members                 = arena_alloc(ctx->sarr_arena);
		type_struct->data.type_strct.is_multiple_return_type = true;
		struct ast *tmp;
		s32         index = 0;
	NEXT:
		tmp = parse_decl_member(ctx, index++);
		if (tmp) {
			if (AST_IS_BAD(tmp)) consume_till(ctx->tokens, SYM_COMMA, SYM_RPAREN);
			sarrput(type_struct->data.type_strct.members, tmp);
			if (tokens_consume_if(ctx->tokens, SYM_COMMA)) goto NEXT;
		}
		struct token *tok = tokens_consume_if(ctx->tokens, SYM_RPAREN);
		if (!tok) {
			report_error(MISSING_BRACKET,
			             tokens_peek(ctx->tokens),
			             CARET_WORD,
			             "Expected end of return list or another return type separated by comma.");
			scope_pop(ctx);
			return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_begin, scope_get(ctx)));
		}
		scope_pop(ctx);
		if (!sarrlenu(type_struct->data.type_strct.members)) {
			report_error(INVALID_TYPE,
			             tok_begin,
			             CARET_WORD,
			             "Expected at least one return type inside parenthesis, if function should "
			             "return 'void' remove parenthesis and leave return type unspecified.");
		}
		return_zone(type_struct);
	}
	return_zone(parse_type(ctx));
}

struct ast *parse_type_fn(struct context *ctx, bool named_args, bool create_scope) {
	zone();
	struct token *tok_fn = tokens_consume_if(ctx->tokens, SYM_FN);
	if (!tok_fn) return_zone(NULL);

	struct token *tok = tokens_consume(ctx->tokens);
	if (tok->sym != SYM_LPAREN) {
		report_error(MISSING_BRACKET, tok, CARET_WORD, "Expected function parameter list.");
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_fn, scope_get(ctx)));
	}
	struct ast *fn = ast_create_node(ctx->ast_arena, AST_TYPE_FN, tok_fn, scope_get(ctx));

	if (create_scope) {
		struct scope *scope = scope_create(ctx->scope_thread_local, SCOPE_FN, scope_get(ctx), &tok_fn->location);
		scope_push(ctx, scope);
	}

	// parse arg types
	bool        rq    = false;
	u32         index = 0;
	struct ast *tmp;
	arrput(ctx->fn_type_stack, fn);
NEXT:
	tmp = parse_decl_arg(ctx, named_args);
	if (tmp) {
		if (tmp->kind == AST_BAD) return tmp;
		// Setup argument index -> order in the function type argument list.
		tmp->data.decl_arg.index = index++;
		if (!fn->data.type_fn.args) {
			fn->data.type_fn.args = arena_alloc(ctx->sarr_arena);
		}
		sarrput(fn->data.type_fn.args, tmp);
		if (tokens_consume_if(ctx->tokens, SYM_COMMA)) {
			rq = true;
			goto NEXT;
		}
	} else if (rq) {
		struct token *tok_err = tokens_peek(ctx->tokens);
		if (tokens_peek_2nd(ctx->tokens)->sym == SYM_RBLOCK) {
			report_error(EXPECTED_NAME, tok_err, CARET_WORD, "Expected type after comma ','.");
			arrpop(ctx->fn_type_stack);
			if (create_scope) scope_pop(ctx);
			return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_fn, scope_get(ctx)));
		}
	}

	tok = tokens_consume(ctx->tokens);
	if (tok->sym != SYM_RPAREN) {
		report_error(MISSING_BRACKET,
		             tok,
		             CARET_WORD,
		             "Expected end of argument type list ')' or another argument separated "
		             "by comma.");
		arrpop(ctx->fn_type_stack);
		if (create_scope) scope_pop(ctx);
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_fn, scope_get(ctx)));
	}
	fn->data.type_fn.ret_type = parse_type_fn_return((ctx));
	arrpop(ctx->fn_type_stack);
	if (create_scope) scope_pop(ctx);
	return_zone(fn);
}

struct ast *parse_type_fn_group(struct context *ctx) {
	zone();
	if (!tokens_is_seq(ctx->tokens, 2, SYM_FN, SYM_LBLOCK)) return_zone(NULL);
	struct token *tok_group = tokens_consume(ctx->tokens); // eat fn
	struct token *tok_begin = tokens_consume(ctx->tokens); // eat {
	struct ast   *group =
	    ast_create_node(ctx->ast_arena, AST_TYPE_FN_GROUP, tok_group, scope_get(ctx));

	ast_nodes_t *variants              = arena_alloc(ctx->sarr_arena);
	group->data.type_fn_group.variants = variants;
	struct ast *tmp;
NEXT:
	if (parse_semicolon(ctx)) goto NEXT;
	if ((tmp = parse_type(ctx))) {
		if (tmp->kind != AST_TYPE_FN) {
			// This check is important, when we decide to remove this, validation should
			// be handled in MIR.
			builder_msg(
			    MSG_ERR, ERR_INVALID_TYPE, tmp->location, CARET_WORD, "Expected function type.");
		}
		sarrput(variants, tmp);
		goto NEXT;
	}

	struct token *tok = tokens_consume_if(ctx->tokens, SYM_RBLOCK);
	if (!tok) {
		tok = tokens_peek_prev(ctx->tokens);
		report_error(EXPECTED_BODY_END, tok, CARET_AFTER, "Expected end of block '}'.");
		report_note(tok_begin, CARET_WORD, "Block starting here.");
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_begin, scope_get(ctx)));
	}
	return_zone(group);
}

struct ast *parse_type_struct(struct context *ctx) {
	zone();
	struct token *tok_struct = tokens_consume_if(ctx->tokens, SYM_STRUCT);
	if (!tok_struct) tok_struct = tokens_consume_if(ctx->tokens, SYM_UNION);
	if (!tok_struct) return_zone(NULL);

	const bool is_union = tok_struct->sym == SYM_UNION;

	// parse flags
	u32         accepted  = is_union ? 0 : HD_COMPILER | HD_BASE | HD_MAYBE_UNUSED;
	u32         flags     = 0;
	struct ast *base_type = NULL;
	while (true) {
		struct ast               *hd_extension;
		enum hash_directive_flags found = HD_NONE;
		hd_extension                    = parse_hash_directive(ctx, accepted, &found, false);
		if (found == HD_BASE) {
			bassert(hd_extension);
			base_type = hd_extension;
		} else if (!hash_directive_to_flags(found, &flags)) {
			break;
		}
		accepted &= ~found;
	}

	struct ast *curr_decl = decl_get(ctx);
	if (curr_decl && curr_decl->kind == AST_DECL_ENTITY) {
		curr_decl->data.decl.flags |= flags;
	}

	struct token *tok = tokens_consume_if(ctx->tokens, SYM_LBLOCK);
	if (!tok) {
		report_error(MISSING_BRACKET, tok_struct, CARET_AFTER, "Expected struct member list.");
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_struct, scope_get(ctx)));
	}

	struct scope *scope = scope_create(ctx->scope_thread_local, SCOPE_TYPE_STRUCT, scope_get(ctx), &tok->location);
	scope_push(ctx, scope);

	struct ast *type_struct =
	    ast_create_node(ctx->ast_arena, AST_TYPE_STRUCT, tok_struct, scope_get(ctx));
	type_struct->data.type_strct.scope     = scope;
	type_struct->data.type_strct.members   = arena_alloc(ctx->sarr_arena);
	type_struct->data.type_strct.base_type = base_type;
	type_struct->data.type_strct.is_union  = is_union;

	// parse members
	struct ast *tmp;
	s32         index = 0;
NEXT:
	if (parse_docs(ctx)) goto NEXT;
	if ((tmp = parse_decl_member(ctx, index))) {
		sarrput(type_struct->data.type_strct.members, tmp);
		++index;
	} else if ((tmp = parse_stmt_using(ctx))) {
		sarrput(type_struct->data.type_strct.members, tmp);
	}

	if (AST_IS_BAD(tmp)) {
		consume_till(ctx->tokens, SYM_SEMICOLON, SYM_RBLOCK);
	}
	if (tokens_consume_if(ctx->tokens, SYM_SEMICOLON)) {
		goto NEXT;
	}
	tok = tokens_consume_if(ctx->tokens, SYM_RBLOCK);
	if (!tok) {
		report_error(MISSING_BRACKET,
		             tokens_peek(ctx->tokens),
		             CARET_WORD,
		             "Expected end of member list '}' or another member separated by semicolon.");
		tokens_consume_till(ctx->tokens, SYM_SEMICOLON);
		scope_pop(ctx);
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_struct, scope_get(ctx)));
	}

	scope_pop(ctx);
	return_zone(type_struct);
}

static enum tokens_lookahead_state cmp_decl(struct token *curr) {
	switch (curr->sym) {
	case SYM_COLON:
		return TOK_LOOK_HIT;
	case SYM_COMMA:
	case SYM_IDENT:
		return TOK_LOOK_CONTINUE;
	default:
		return TOK_LOOK_TERMINAL;
	}
}

struct ast *parse_decl(struct context *ctx) {
	zone();
	// is value declaration?
	if (!tokens_lookahead(ctx->tokens, cmp_decl)) return_zone(NULL);
	struct token *tok_begin = tokens_peek(ctx->tokens);
	struct ast   *ident     = parse_ident_group(ctx);
	if (!ident) return_zone(NULL);
	// eat :
	tokens_consume(ctx->tokens);

	struct ast *decl           = ast_create_node(ctx->ast_arena, AST_DECL_ENTITY, tok_begin, scope_get(ctx));
	decl->docs                 = pop_docs(ctx);
	decl->data.decl.name       = ident;
	decl->data.decl_entity.mut = true;

	decl_push(ctx, decl);

	decl->data.decl.type     = parse_type(ctx);
	struct token *tok_assign = tokens_consume_if(ctx->tokens, SYM_ASSIGN);
	if (!tok_assign) tok_assign = tokens_consume_if(ctx->tokens, SYM_COLON);

	// Parse hash directives.
	s32 hd_accepted = HD_COMPILER | HD_THREAD_LOCAL | HD_MAYBE_UNUSED;

	if (tok_assign) {
		decl->data.decl_entity.mut = token_is(tok_assign, SYM_ASSIGN);

		// parse declaration expression
		decl->data.decl_entity.value = parse_expr(ctx);

		if (isnotflag(decl->data.decl.flags, FLAG_EXTERN)) {
			if (!decl->data.decl_entity.value) {
				report_error(EXPECTED_INITIALIZATION,
				             tok_assign,
				             CARET_AFTER,
				             "Expected binding of declaration to some value.");
				decl_pop(ctx);
				return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok_begin, scope_get(ctx)));
			}
		}
	} else {
		hd_accepted |= HD_NO_INIT;
	}

	struct ast *init_value = decl->data.decl_entity.value;

	if (!init_value || rq_semicolon_after_decl_entity(init_value)) {
		u32 flags = 0;
		while (true) {
			enum hash_directive_flags found = HD_NONE;
			parse_hash_directive(ctx, hd_accepted, &found, false);
			if (!hash_directive_to_flags(found, &flags)) break;
			hd_accepted &= ~found;
		}

		if (isflag(flags, FLAG_NO_INIT) && !scope_is_local(scope_get(ctx))) {
			report_error(EXPECTED_INITIALIZATION,
			             tok_begin,
			             CARET_AFTER,
			             "Invalid 'noinit' directive for global variable '%s'. All globals must "
			             "be initialized either by explicit value or implicit default value.",
			             ident->data.ident.id.str);
		}

		decl->data.decl.flags |= flags;
	}

	decl_pop(ctx);
	return_zone(decl);
}

struct ast *parse_expr_call(struct context *ctx, struct ast *prev) {
	zone();
	if (!prev) return_zone(NULL);

	struct token *location_token = tokens_peek_prev(ctx->tokens);
	struct token *tok            = tokens_consume_if(ctx->tokens, SYM_LPAREN);
	if (!tok) return_zone(NULL);
	if (location_token && location_token->sym != SYM_IDENT) location_token = tok;
	struct ast *call =
	    ast_create_node(ctx->ast_arena, AST_EXPR_CALL, location_token, scope_get(ctx));
	call->data.expr_call.ref = prev;
	// parse args
	bool        rq = false;
	struct ast *tmp;
arg:
	tmp = parse_expr(ctx);
	if (tmp) {
		if (!call->data.expr_call.args) {
			call->data.expr_call.args = arena_alloc(ctx->sarr_arena);
		}
		sarrput(call->data.expr_call.args, tmp);

		if (tokens_consume_if(ctx->tokens, SYM_COMMA)) {
			rq = true;
			goto arg;
		}
	} else if (rq) {
		struct token *tok_err = tokens_peek(ctx->tokens);
		if (tokens_peek_2nd(ctx->tokens)->sym == SYM_RBLOCK) {
			report_error(
			    EXPECTED_NAME, tok_err, CARET_WORD, "Expected function argument after comma ','.");
			return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok, scope_get(ctx)));
		}
	}

	tok = tokens_consume(ctx->tokens);
	if (tok->sym != SYM_RPAREN) {
		report_error(MISSING_BRACKET,
		             tok,
		             CARET_WORD,
		             "Expected end of parameter list ')' or another parameter separated by comma.");
		return_zone(ast_create_node(ctx->ast_arena, AST_BAD, tok, scope_get(ctx)));
	}

	return_zone(call);
}

struct ast *parse_expr_null(struct context *ctx) {
	struct token *tok_null = tokens_consume_if(ctx->tokens, SYM_NULL);
	if (!tok_null) return NULL;
	return ast_create_node(ctx->ast_arena, AST_EXPR_NULL, tok_null, scope_get(ctx));
}

struct ast *parse_unrecheable(struct context *ctx) {
	struct token *tok = tokens_consume_if(ctx->tokens, SYM_UNREACHABLE);
	if (!tok) return NULL;

	return ast_create_node(ctx->ast_arena, AST_UNREACHABLE, tok, scope_get(ctx));
}

struct ast *parse_debugbreak(struct context *ctx) {
	struct token *tok = tokens_consume_if(ctx->tokens, SYM_DEBUGBREAK);
	if (!tok) return NULL;
	return ast_create_node(ctx->ast_arena, AST_DEBUGBREAK, tok, scope_get(ctx));
}

struct ast *parse_expr_type(struct context *ctx) {
	struct token *tok  = tokens_peek(ctx->tokens);
	struct ast   *type = NULL;

	type = parse_type_struct(ctx);

	// keep order
	if (!type) type = parse_type_slice(ctx);
	if (!type) type = parse_type_dynarr(ctx);
	if (!type) type = parse_type_arr(ctx);
	// keep order

	if (!type) type = parse_type_enum(ctx);
	if (!type) type = parse_type_ptr(ctx);

	if (type) {
		struct ast *expr          = ast_create_node(ctx->ast_arena, AST_EXPR_TYPE, tok, scope_get(ctx));
		expr->data.expr_type.type = type;
		return expr;
	}

	return NULL;
}

struct ast *parse_single_block_stmt_or_expr(struct context *ctx, bool *out_require_semicolon) {
	struct token *tok;
	struct ast   *tmp = NULL;

	if (out_require_semicolon) *out_require_semicolon = false;

NEXT:
	switch (tokens_peek_sym(ctx->tokens)) {
	case SYM_SEMICOLON:
		tok = tokens_consume(ctx->tokens);
		if (builder.errorc == 0) {
			report_warning(tok, CARET_WORD, "Extra semicolon can be removed ';'.");
		}
		goto NEXT;

	case SYM_HASH: {
		enum hash_directive_flags satisfied;
		tmp = parse_hash_directive(ctx, HD_STATIC_IF, &satisfied, false);
		break;
	}
	case SYM_RETURN:
		tmp = parse_stmt_return(ctx);
		if (out_require_semicolon && AST_IS_OK(tmp)) *out_require_semicolon = true;
		if (tmp->kind == AST_STMT_RETURN) {
			struct ast *owner_block = block_get(ctx);
			bassert(owner_block && owner_block->kind == AST_BLOCK);
			tmp->data.stmt_return.owner_block  = owner_block;
			owner_block->data.block.has_return = true;
		}
		break;
	case SYM_USING:
		tmp = parse_stmt_using(ctx);
		if (out_require_semicolon && AST_IS_OK(tmp)) *out_require_semicolon = true;
		break;
	case SYM_IF:
		tmp = parse_stmt_if(ctx, false);
		break;
	case SYM_SWITCH:
		tmp = parse_stmt_switch(ctx);
		break;
	case SYM_LOOP:
		tmp = parse_stmt_loop(ctx);
		break;
	case SYM_BREAK:
		tmp = parse_stmt_break(ctx);
		if (out_require_semicolon) *out_require_semicolon = true;
		break;
	case SYM_CONTINUE:
		tmp = parse_stmt_continue(ctx);
		if (out_require_semicolon) *out_require_semicolon = true;
		break;
	case SYM_DEFER:
		tmp = parse_stmt_defer(ctx);
		if (out_require_semicolon && AST_IS_OK(tmp)) *out_require_semicolon = true;
		break;
	case SYM_LBLOCK:
		tmp = parse_block(ctx, SCOPE_LEXICAL);
		break;
	case SYM_UNREACHABLE:
		tmp = parse_unrecheable(ctx);
		if (out_require_semicolon) *out_require_semicolon = true;
		break;
	case SYM_DEBUGBREAK:
		tmp = parse_debugbreak(ctx);
		if (out_require_semicolon) *out_require_semicolon = true;
		break;
	default:
		break;
	}

	if (tmp) return tmp;

	// Others
	if ((tmp = (struct ast *)parse_decl(ctx))) {
		if (out_require_semicolon && AST_IS_OK(tmp)) *out_require_semicolon = true;
		return tmp;
	}
	if ((tmp = parse_expr(ctx))) {
		if (out_require_semicolon && AST_IS_OK(tmp)) *out_require_semicolon = true;
		return tmp;
	}

	return NULL;
}

struct ast *parse_block(struct context *ctx, enum scope_kind scope_kind) {
	struct token *tok_begin = tokens_consume_if(ctx->tokens, SYM_LBLOCK);
	if (!tok_begin) return NULL;

	bool scope_created = false;
	if (scope_kind != SCOPE_NONE) {
		bassert((scope_kind == SCOPE_LEXICAL || scope_kind == SCOPE_FN_BODY) &&
		        "Unexpected scope kind, extend this assert in case it's intentional.");

		struct scope *scope = scope_create(ctx->scope_thread_local, scope_kind, scope_get(ctx), &tok_begin->location);

		scope_push(ctx, scope);
		scope_created = true;
	}
	struct ast *block = ast_create_node(ctx->ast_arena, AST_BLOCK, tok_begin, scope_get(ctx));
	struct ast *tmp   = NULL;

	block->data.block.nodes = arena_alloc(ctx->sarr_arena);

	block_push(ctx, block);

	bool require_semicolon = false;
	while ((tmp = parse_single_block_stmt_or_expr(ctx, &require_semicolon))) {
		sarrput(block->data.block.nodes, tmp);
		if (require_semicolon && !parse_semicolon_rq(ctx)) {
			consume_till(ctx->tokens, SYM_RBLOCK, SYM_SEMICOLON);
			tokens_consume_if(ctx->tokens, SYM_SEMICOLON);
		}
	}

	struct token *tok = tokens_consume_if(ctx->tokens, SYM_RBLOCK);
	if (!tok) {
		tok = tokens_peek_prev(ctx->tokens);
		report_error(EXPECTED_BODY_END, tok, CARET_AFTER, "Expected end of block '}'.");
		report_note(tok_begin, CARET_WORD, "Block starting here.");
		if (scope_created) scope_pop(ctx);
		block_pop(ctx);
		return ast_create_node(ctx->ast_arena, AST_BAD, tok_begin, scope_get(ctx));
	}

	if (scope_created) scope_pop(ctx);
	block_pop(ctx);
	return block;
}

void parse_ublock_content(struct context *ctx, struct ast *ublock) {
	bassert(ublock->kind == AST_UBLOCK);
	arrsetcap(ublock->data.ublock.nodes, 64);
	struct ast *tmp;
NEXT:
	if (parse_semicolon(ctx)) goto NEXT;
	if (parse_docs(ctx)) goto NEXT;
	if (parse_unit_docs(ctx)) goto NEXT;

	if ((tmp = parse_decl(ctx))) {
		if (AST_IS_OK(tmp)) {
			struct ast *decl = tmp->data.decl_entity.value;
			if (decl && rq_semicolon_after_decl_entity(decl)) {
				if (!parse_semicolon_rq(ctx)) {
					consume_till(ctx->tokens, SYM_SEMICOLON, SYM_IDENT, SYM_HASH);
					tokens_consume_if(ctx->tokens, SYM_SEMICOLON);
				}
			}
			// setup global scope flag for declaration
			tmp->data.decl_entity.is_global = true; // @Incomplete 2024-12-12 Use decl flags here too?
		}

		arrput(ublock->data.ublock.nodes, tmp);
		goto NEXT;
	}

	// load, import, link, test, private - enabled in global scope
	const int enabled_hd = HD_LOAD | HD_PRIVATE | HD_IMPORT | HD_SCOPE_PRIVATE | HD_SCOPE_PUBLIC | HD_SCOPE_MODULE;
	if ((tmp = parse_hash_directive(ctx, enabled_hd, NULL, false))) {
		arrput(ublock->data.ublock.nodes, tmp);
		goto NEXT;
	}

	bassert(ctx->unit->ublock_ast == NULL);
	ctx->unit->ublock_ast = ublock->data.ublock.nodes;

	struct token *tok = tokens_peek(ctx->tokens);
	if (!token_is(tok, SYM_EOF)) {
		report_error(UNEXPECTED_SYMBOL,
		             tok,
		             CARET_WORD,
		             "Unexpected symbol in module body '%s'.",
		             sym_strings[tok->sym]);
	}
}

void init_hash_directives(struct context *ctx) {
	static const char *hash_directive_names[] = {
#define HD_GEN(kind, name, flag) name,
#include "parser.def"
#undef HD_GEN
	};

	static s32 hash_directive_flags[static_arrlenu(hash_directive_names)] = {
#define HD_GEN(kind, name, flag) flag,
#include "parser.def"
#undef HD_GEN
	};

	for (usize i = 0; i < static_arrlenu(hash_directive_names); ++i) {
		const hash_t                hash  = strhash(make_str_from_c(hash_directive_names[i]));
		struct hash_directive_entry entry = (struct hash_directive_entry){
		    .hash  = hash,
		    .value = hash_directive_flags[i],
		};
		tbl_insert(ctx->hash_directive_table, entry);
	}
}

void parser_run(struct assembly *assembly, struct unit *unit) {
	bassert(assembly);
	bassert(assembly->gscope && "Missing global scope for assembly.");

	const u32 thread_index = get_worker_index();

	zone();
	runtime_measure_begin(parse);

	struct context ctx = {
	    .assembly = assembly,
	    .unit     = unit,
	    .tokens   = &unit->tokens,

	    .ast_arena          = &assembly->thread_local_contexts[thread_index].ast_arena,
	    .scope_thread_local = &assembly->thread_local_contexts[thread_index].scope_thread_local,
	    .sarr_arena         = &assembly->thread_local_contexts[thread_index].small_array,
	    .string_cache       = &assembly->thread_local_contexts[thread_index].string_cache,
	};

	init_hash_directives(&ctx);
	bassert(unit->parent_scope);
	scope_push(&ctx, unit->parent_scope);

	struct ast *root       = ast_create_node(ctx.ast_arena, AST_UBLOCK, NULL, scope_get(&ctx));
	root->data.ublock.unit = unit;
	unit->ast              = root;

	parse_ublock_content(&ctx, unit->ast);
	unit->ast->docs = str_buf_view(unit->file_docs_cache);

	tbl_free(ctx.hash_directive_table);
	arrfree(ctx.decl_stack);
	arrfree(ctx.scope_stack);
	arrfree(ctx.fn_type_stack);
	arrfree(ctx.block_stack);

	batomic_fetch_add_s32(&assembly->stats.parsing_ms, runtime_measure_end(parse));
	return_zone();
}
