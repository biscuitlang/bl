#include "ast.h"
#include "builder.h"
#include "stb_ds.h"
#include <stdio.h>

static inline void print_address(struct ast *node, FILE *stream) {
#ifdef BL_DEBUG
	if (node)
		fprintf(stream, " %llu ", node->_serial);
	else
		fprintf(stream, " (null) ");
#else
	fprintf(stream, " %p ", node);
#endif
}

#define print_head(_node, _pad, _stream) _print_head((struct ast *)(_node), (_pad), (_stream))

static inline void _print_head(struct ast *node, s32 pad, FILE *stream) {
	if (node->location)
		fprintf(stream,
		        "\n%*s%s <%d:%d>",
		        pad * 2,
		        "",
		        ast_get_name(node),
		        node->location->line,
		        node->location->col);
	else
		fprintf(stream, "\n%*s%s <IMPLICIT>", pad * 2, "", ast_get_name(node));

	print_address(node, stream);
}

static inline void print_flags(u32 flags, FILE *stream) {
	if (!flags) return;
	if (isflag(flags, FLAG_EXTERN)) fprintf(stream, " #extern");
	if (isflag(flags, FLAG_TEST_FN)) fprintf(stream, " #test");
	if (isflag(flags, FLAG_COMPILER)) fprintf(stream, " #compiler");
}

static void print_node(struct ast *node, s32 pad, FILE *stream);
static void print_ublock(struct ast *ublock, s32 pad, FILE *stream);
static void print_load(struct ast *load, s32 pad, FILE *stream);
static void print_import(struct ast *import, s32 pad, FILE *stream);
static void print_link(struct ast *link, s32 pad, FILE *stream);
static void print_private(struct ast *private, s32 pad, FILE *stream);
static void print_module_private(struct ast *private, s32 pad, FILE *stream);
static void print_public(struct ast *private, s32 pad, FILE *stream);
static void print_call_loc(struct ast *call_loc, s32 pad, FILE *stream);
static void print_err(struct ast *err, s32 pad, FILE *stream);
static void print_block(struct ast *block, s32 pad, FILE *stream);
static void print_unrecheable(struct ast *unr, s32 pad, FILE *stream);
static void print_debugbreak(struct ast *debug_break, s32 pad, FILE *stream);
static void print_ref(struct ast *ref, s32 pad, FILE *stream);
static void print_type_polymorph(struct ast *poly, s32 pad, FILE *stream);
static void print_type_struct(struct ast *strct, s32 pad, FILE *stream);
static void print_type_slice(struct ast *slice, s32 pad, FILE *stream);
static void print_type_enum(struct ast *enm, s32 pad, FILE *stream);
static void print_type_fn_group(struct ast *group, s32 pad, FILE *stream);
static void print_type_fn(struct ast *fn, s32 pad, FILE *stream);
static void print_stmt_if(struct ast *stmt_if, s32 pad, FILE *stream);
static void print_stmt_switch(struct ast *stmt_switch, s32 pad, FILE *stream);
static void print_stmt_case(struct ast *stmt_case, s32 pad, FILE *stream);
static void print_stmt_loop(struct ast *loop, s32 pad, FILE *stream);
static void print_stmt_break(struct ast *br, s32 pad, FILE *stream);
static void print_stmt_continue(struct ast *ctx, s32 pad, FILE *stream);
static void print_stmt_using(struct ast *using, s32 pad, FILE *stream);
static void print_stmt_return(struct ast *ret, s32 pad, FILE *stream);
static void print_stmt_defer(struct ast *defer, s32 pad, FILE *stream);
static void print_decl_entity(struct ast *entity, s32 pad, FILE *stream);
static void print_decl_arg(struct ast *arg, s32 pad, FILE *stream);
static void print_decl_member(struct ast *member, s32 pad, FILE *stream);
static void print_decl_variant(struct ast *variant, s32 pad, FILE *stream);
static void print_bad(struct ast *bad, s32 pad, FILE *stream);
static void print_expr_unary(struct ast *unary, s32 pad, FILE *stream);
static void print_expr_cast(struct ast *cast, s32 pad, FILE *stream);
static void print_expr_addrof(struct ast *addrof, s32 pad, FILE *stream);
static void print_expr_test_cases(struct ast *type_info, s32 pad, FILE *stream);
static void print_expr_deref(struct ast *deref, s32 pad, FILE *stream);
static void print_expr_binop(struct ast *binop, s32 pad, FILE *stream);
static void print_expr_type(struct ast *expr_type, s32 pad, FILE *stream);
static void print_expr_compound(struct ast *expr_compound, s32 pad, FILE *stream);
static void print_expr_lit_int(struct ast *lit, s32 pad, FILE *stream);
static void print_expr_lit_float(struct ast *lit, s32 pad, FILE *stream);
static void print_expr_lit_double(struct ast *lit, s32 pad, FILE *stream);
static void print_expr_lit_char(struct ast *lit, s32 pad, FILE *stream);
static void print_expr_lit_bool(struct ast *lit, s32 pad, FILE *stream);
static void print_expr_lit_string(struct ast *lit, s32 pad, FILE *stream);
static void print_expr_lit_fn(struct ast *fn, s32 pad, FILE *stream);
static void print_expr_lit_fn_group(struct ast *group, s32 pad, FILE *stream);
static void print_expr_call(struct ast *call, s32 pad, FILE *stream);
static void print_expr_catch(struct ast *catch, s32 pad, FILE *stream);
static void print_expr_elem(struct ast *elem, s32 pad, FILE *stream);

// impl
void print_ublock(struct ast *ublock, s32 pad, FILE *stream) {
	print_head(ublock, pad, stream);
	const str_t unit_name = ublock->data.ublock.unit->name;
	fprintf(stream, STR_FMT, STR_ARG(unit_name));
	for (usize i = 0; i < arrlenu(ublock->data.ublock.nodes); ++i) {
		print_node(ublock->data.ublock.nodes[i], pad + 1, stream);
	}
}

void print_block(struct ast *block, s32 pad, FILE *stream) {
	print_head(block, pad, stream);
	for (usize i = 0; i < sarrlenu(block->data.block.nodes); ++i) {
		struct ast *tmp = sarrpeek(block->data.block.nodes, i);
		print_node(tmp, pad + 1, stream);
	}
}

void print_load(struct ast *load, s32 pad, FILE *stream) {
	print_head(load, pad, stream);
	const str_t filepath = load->data.load.filepath;
	fprintf(stream, STR_FMT, STR_ARG(filepath));
}

void print_import(struct ast *import, s32 pad, FILE *stream) {
	print_head(import, pad, stream);
	struct module *module = import->data.import.module;
	if (module) {
		fprintf(stream, STR_FMT, STR_ARG(module->modulepath));
	}
}

void print_link(struct ast *link, s32 pad, FILE *stream) {
	print_head(link, pad, stream);
	fprintf(stream, "%s", link->data.link.lib);
}

void print_private(struct ast *private, s32 pad, FILE *stream) {
	print_head(private, pad, stream);
}

void print_module_private(struct ast *module, s32 pad, FILE *stream) {
	print_head(module, pad, stream);
}

void print_public(struct ast *public, s32 pad, FILE *stream) {
	print_head(public, pad, stream);
}

void print_call_loc(struct ast *call_loc, s32 pad, FILE *stream) {
	print_head(call_loc, pad, stream);
}

void print_err(struct ast *err, s32 pad, FILE *stream) {
	print_head(err, pad, stream);
}

void print_unrecheable(struct ast *unr, s32 pad, FILE *stream) {
	print_head(unr, pad, stream);
}

void print_debugbreak(struct ast *debug_break, s32 pad, FILE *stream) {
	print_head(debug_break, pad, stream);
}

void print_type_polymorph(struct ast *poly, s32 pad, FILE *stream) {
	print_head(poly, pad, stream);
	struct ast *ident = poly->data.type_poly.ident;
	if (ident) {
		const str_t s = ident->data.ident.id.str;
		fprintf(stream, "'" STR_FMT "' ", STR_ARG(s));
	}
}

void print_type_struct(struct ast *strct, s32 pad, FILE *stream) {
	print_head(strct, pad, stream);

	ast_nodes_t *members = strct->data.type_strct.members;
	for (usize i = 0; i < sarrlenu(members); ++i) {
		struct ast *member = sarrpeek(members, i);
		print_node(member, pad + 1, stream);
	}
}

void print_type_slice(struct ast *slice, s32 pad, FILE *stream) {
	print_head(slice, pad, stream);
	print_node(slice->data.type_slice.elem_type, pad + 1, stream);
}

void print_ref(struct ast *ref, s32 pad, FILE *stream) {
	print_head(ref, pad, stream);

	struct ast *ident = ref->data.ref.ident;
	if (ident) {
		const str_t s = ident->data.ident.id.str;
		fprintf(stream, "'" STR_FMT "' ", STR_ARG(s));
	}

	struct ast *next = ref->data.ref.next;
	if (next) print_node(next, pad + 1, stream);
}

void print_type_fn_group(struct ast *group, s32 pad, FILE *stream) {
	print_head(group, pad, stream);

	ast_nodes_t *variants = group->data.type_fn_group.variants;
	for (usize i = 0; i < sarrlenu(variants); ++i) {
		struct ast *node = sarrpeek(variants, i);
		print_node(node, pad + 1, stream);
	}
}

void print_type_fn(struct ast *fn, s32 pad, FILE *stream) {
	print_head(fn, pad, stream);
	ast_nodes_t *args = fn->data.type_fn.args;
	for (usize i = 0; i < sarrlenu(args); ++i) {
		struct ast *node = sarrpeek(args, i);
		print_node(node, pad + 1, stream);
	}
	print_node(fn->data.type_fn.ret_type, pad + 1, stream);
}

void print_type_enum(struct ast *enm, s32 pad, FILE *stream) {
	print_head(enm, pad, stream);
	ast_nodes_t *variants = enm->data.type_enm.variants;
	for (usize i = 0; i < sarrlenu(variants); ++i) {
		struct ast *node = sarrpeek(variants, i);
		print_node(node, pad + 1, stream);
	}
}

void print_stmt_if(struct ast *stmt_if, s32 pad, FILE *stream) {
	print_head(stmt_if, pad, stream);
	print_node(stmt_if->data.stmt_if.test, pad + 1, stream);
	print_node(stmt_if->data.stmt_if.true_stmt, pad + 1, stream);
	print_node(stmt_if->data.stmt_if.false_stmt, pad + 1, stream);
}

void print_stmt_switch(struct ast *stmt_switch, s32 pad, FILE *stream) {
	print_head(stmt_switch, pad, stream);
	print_node(stmt_switch->data.stmt_switch.expr, pad + 1, stream);

	ast_nodes_t *cases = stmt_switch->data.stmt_switch.cases;
	for (usize i = 0; i < sarrlenu(cases); ++i) {
		struct ast *stmt_case = sarrpeek(cases, i);
		print_node(stmt_case, pad + 1, stream);
	}
}

void print_stmt_case(struct ast *stmt_case, s32 pad, FILE *stream) {
	print_head(stmt_case, pad, stream);
	if (stmt_case->data.stmt_case.is_default) fprintf(stream, "default");

	ast_nodes_t *exprs = stmt_case->data.stmt_case.exprs;
	for (usize i = 0; i < sarrlenu(exprs); ++i) {
		struct ast *expr = sarrpeek(exprs, i);
		print_node(expr, pad + 1, stream);
	}

	if (stmt_case->data.stmt_case.block) {
		print_node(stmt_case->data.stmt_case.block, pad + 1, stream);
	}
}

void print_stmt_loop(struct ast *loop, s32 pad, FILE *stream) {
	print_head(loop, pad, stream);
	print_node(loop->data.stmt_loop.init, pad + 1, stream);
	print_node(loop->data.stmt_loop.condition, pad + 1, stream);
	print_node(loop->data.stmt_loop.increment, pad + 1, stream);
	print_node(loop->data.stmt_loop.block, pad + 1, stream);
}

void print_stmt_break(struct ast *br, s32 pad, FILE *stream) {
	print_head(br, pad, stream);
}

void print_stmt_continue(struct ast *ctx, s32 pad, FILE *stream) {
	print_head(ctx, pad, stream);
}

void print_stmt_using(struct ast *using, s32 pad, FILE *stream) {
	print_head(using, pad, stream);
	print_node(using->data.stmt_using.scope_expr, pad + 1, stream);
}

void print_stmt_return(struct ast *ret, s32 pad, FILE *stream) {
	print_head(ret, pad, stream);
	for (usize i = 0; i < sarrlenu(ret->data.stmt_return.exprs); ++i) {
		struct ast *value = sarrpeek(ret->data.stmt_return.exprs, i);
		print_node(value, pad + 1, stream);
	}
}

void print_stmt_defer(struct ast *defer, s32 pad, FILE *stream) {
	print_head(defer, pad, stream);
	print_node(defer->data.stmt_defer.expr, pad + 1, stream);
}

void print_decl_entity(struct ast *entity, s32 pad, FILE *stream) {
	print_head(entity, pad, stream);

	const str_t name = entity->data.decl.name->data.ident.id.str;
	fprintf(stream,
	        "'" STR_FMT "' '%s'",
	        STR_ARG(name),
	        entity->data.decl_entity.mut ? "mutable" : "immutable");

	print_flags(entity->data.decl.flags, stream);
	print_node((struct ast *)entity->data.decl.type, pad + 1, stream);
	print_node((struct ast *)entity->data.decl_entity.value, pad + 1, stream);
}

void print_decl_arg(struct ast *arg, s32 pad, FILE *stream) {
	print_head(arg, pad, stream);
	const str_t name = arg->data.decl.name->data.ident.id.str;
	fprintf(stream, "'" STR_FMT "'", STR_ARG(name));
	print_node(arg->data.decl.type, pad + 1, stream);
}

void print_decl_member(struct ast *member, s32 pad, FILE *stream) {
	print_head(member, pad, stream);
	const str_t name = member->data.decl.name->data.ident.id.str;
	fprintf(stream, "'" STR_FMT "'", STR_ARG(name));
	print_node(member->data.decl.type, pad + 1, stream);
}

void print_decl_variant(struct ast *variant, s32 pad, FILE *stream) {
	print_head(variant, pad, stream);
	const str_t name = variant->data.decl.name->data.ident.id.str;
	fprintf(stream, "'" STR_FMT "'", STR_ARG(name));
	print_node(variant->data.decl.type, pad + 1, stream);
}

void print_bad(struct ast *bad, s32 pad, FILE *stream) {
	print_head(bad, pad, stream);
}

void print_expr_cast(struct ast *cast, s32 pad, FILE *stream) {
	print_head(cast, pad, stream);
	if (cast->data.expr_cast.auto_cast) {
		fprintf(stream, "<auto>");
	} else {
		print_node(cast->data.expr_cast.type, pad + 1, stream);
	}
	print_node(cast->data.expr_cast.next, pad + 1, stream);
}

void print_expr_unary(struct ast *unary, s32 pad, FILE *stream) {
	print_head(unary, pad, stream);

	const char *op = NULL;
	switch (unary->data.expr_unary.kind) {
	case UNOP_INVALID:
		op = "invalid";
		break;
	case UNOP_NEG:
		op = "-";
		break;
	case UNOP_POS:
		op = "+";
		break;
	case UNOP_NOT:
		op = "!";
		break;
	case UNOP_BIT_NOT:
		op = "~";
		break;
	}

	fprintf(stream, "'%s' ", op);
	print_node(unary->data.expr_unary.next, pad + 1, stream);
}

void print_expr_addrof(struct ast *addrof, s32 pad, FILE *stream) {
	print_head(addrof, pad, stream);
	print_node(addrof->data.expr_addrof.next, pad + 1, stream);
}

void print_expr_test_cases(struct ast *type_info, s32 pad, FILE *stream) {
	print_head(type_info, pad, stream);
}

void print_expr_deref(struct ast *deref, s32 pad, FILE *stream) {
	print_head(deref, pad, stream);
	print_node(deref->data.expr_deref.next, pad + 1, stream);
}

void print_expr_elem(struct ast *elem, s32 pad, FILE *stream) {
	print_head(elem, pad, stream);
	print_node(elem->data.expr_elem.index, pad + 1, stream);
	print_node(elem->data.expr_elem.next, pad + 1, stream);
}

void print_expr_binop(struct ast *binop, s32 pad, FILE *stream) {
	print_head(binop, pad, stream);
	fprintf(stream, "'%s' ", ast_binop_to_str(binop->data.expr_binop.kind));
	print_node(binop->data.expr_binop.lhs, pad + 1, stream);
	print_node(binop->data.expr_binop.rhs, pad + 1, stream);
}

void print_expr_type(struct ast *expr_type, s32 pad, FILE *stream) {
	print_head(expr_type, pad, stream);
	print_node(expr_type->data.expr_type.type, pad + 1, stream);
}

void print_expr_lit_int(struct ast *lit, s32 pad, FILE *stream) {
	print_head(lit, pad, stream);
	fprintf(stream, "%llu ", (long long unsigned)lit->data.expr_integer.val);
}

void print_expr_lit_float(struct ast *lit, s32 pad, FILE *stream) {
	print_head(lit, pad, stream);
	fprintf(stream, "%f ", lit->data.expr_float.val);
}

void print_expr_lit_double(struct ast *lit, s32 pad, FILE *stream) {
	print_head(lit, pad, stream);
	fprintf(stream, "%f ", lit->data.expr_double.val);
}

void print_expr_lit_char(struct ast *lit, s32 pad, FILE *stream) {
	print_head(lit, pad, stream);
	fprintf(stream, "%c ", lit->data.expr_character.val);
}

void print_expr_lit_bool(struct ast *lit, s32 pad, FILE *stream) {
	print_head(lit, pad, stream);
	fprintf(stream, "%s ", lit->data.expr_boolean.val ? "true" : "false");
}

void print_expr_lit_string(struct ast *lit, s32 pad, FILE *stream) {
	print_head(lit, pad, stream);

	char *tmp = strdup(lit->data.expr_string.val.ptr);
	fprintf(stream, "%s ", strtok(tmp, "\n"));
	char *next = strtok(NULL, "\n");
	if (next && strlen(next)) fprintf(stream, "... ");
	free(tmp);
}

void print_expr_lit_fn(struct ast *fn, s32 pad, FILE *stream) {
	print_head(fn, pad, stream);
	print_node(fn->data.expr_fn.type, pad + 1, stream);
	print_node(fn->data.expr_fn.enable_if, pad + 1, stream);
	print_node(fn->data.expr_fn.block, pad + 1, stream);
}

void print_expr_lit_fn_group(struct ast *group, s32 pad, FILE *stream) {
	print_head(group, pad, stream);
	ast_nodes_t *variants = group->data.expr_fn_group.variants;
	for (usize i = 0; i < sarrlenu(variants); ++i) {
		struct ast *tmp = sarrpeek(variants, i);
		print_node(tmp, pad + 1, stream);
	}
}

void print_expr_call(struct ast *call, s32 pad, FILE *stream) {
	print_head(call, pad, stream);

	print_node(call->data.expr_call.ref, pad + 1, stream);

	ast_nodes_t *args = call->data.expr_call.args;
	for (usize i = 0; i < sarrlenu(args); ++i) {
		struct ast *arg = sarrpeek(args, i);
		print_node(arg, pad + 1, stream);
	}
}

void print_expr_catch(struct ast *catch, s32 pad, FILE *stream) {
	print_head(catch, pad, stream);
	print_node(catch->data.expr_catch.call, pad + 1, stream);
	print_node(catch->data.expr_catch.block, pad + 1, stream);
}

void print_expr_compound(struct ast *expr_compound, s32 pad, FILE *stream) {
	print_head(expr_compound, pad, stream);

	if (expr_compound->data.expr_compound.type) {
		print_node(expr_compound->data.expr_compound.type, pad + 1, stream);
	}

	ast_nodes_t *exprs = expr_compound->data.expr_compound.values;
	for (usize i = 0; i < sarrlenu(exprs); ++i) {
		struct ast *value = sarrpeek(exprs, i);
		print_node(value, pad + 1, stream);
	}
}

void print_node(struct ast *node, s32 pad, FILE *stream) {
	if (!node) return;
	switch (node->kind) {
	case AST_BAD:
		print_bad(node, pad, stream);
		break;

	case AST_LOAD:
		print_load(node, pad, stream);
		break;

	case AST_IMPORT:
		print_import(node, pad, stream);
		break;

	case AST_LINK:
		print_link(node, pad, stream);
		break;

	case AST_PRIVATE:
		print_private(node, pad, stream);
		break;

	case AST_MODULE_PRIVATE:
		print_module_private(node, pad, stream);
		break;

	case AST_PUBLIC:
		print_public(node, pad, stream);
		break;

	case AST_CALL_LOC:
		print_call_loc(node, pad, stream);
		break;

	case AST_EXPR_ERR:
		print_err(node, pad, stream);
		break;

	case AST_IDENT:
		break;

	case AST_UBLOCK:
		print_ublock(node, pad, stream);
		break;

	case AST_BLOCK:
		print_block(node, pad, stream);
		break;

	case AST_UNREACHABLE:
		print_unrecheable(node, pad, stream);
		break;

	case AST_DEBUGBREAK:
		print_debugbreak(node, pad, stream);
		break;

	case AST_TYPE_STRUCT:
		print_type_struct(node, pad, stream);
		break;

	case AST_TYPE_SLICE:
		print_type_slice(node, pad, stream);
		break;

	case AST_TYPE_FN:
		print_type_fn(node, pad, stream);
		break;

	case AST_TYPE_ENUM:
		print_type_enum(node, pad, stream);
		break;

	case AST_TYPE_FN_GROUP:
		print_type_fn_group(node, pad, stream);
		break;

	case AST_TYPE_POLY:
		print_type_polymorph(node, pad, stream);
		break;

	case AST_REF:
		print_ref(node, pad, stream);
		break;

	case AST_DECL_ENTITY:
		print_decl_entity(node, pad, stream);
		break;

	case AST_DECL_ARG:
		print_decl_arg(node, pad, stream);
		break;

	case AST_DECL_MEMBER:
		print_decl_member(node, pad, stream);
		break;

	case AST_DECL_VARIANT:
		print_decl_variant(node, pad, stream);
		break;

	case AST_STMT_RETURN:
		print_stmt_return(node, pad, stream);
		break;

	case AST_STMT_USING:
		print_stmt_using(node, pad, stream);
		break;

	case AST_STMT_DEFER:
		print_stmt_defer(node, pad, stream);
		break;

	case AST_STMT_IF:
		print_stmt_if(node, pad, stream);
		break;

	case AST_STMT_SWITCH:
		print_stmt_switch(node, pad, stream);
		break;

	case AST_STMT_CASE:
		print_stmt_case(node, pad, stream);
		break;

	case AST_STMT_LOOP:
		print_stmt_loop(node, pad, stream);
		break;

	case AST_STMT_BREAK:
		print_stmt_break(node, pad, stream);
		break;

	case AST_STMT_CONTINUE:
		print_stmt_continue(node, pad, stream);
		break;

	case AST_EXPR_COMPOUND:
		print_expr_compound(node, pad, stream);
		break;

	case AST_EXPR_TYPE:
		print_expr_type(node, pad, stream);
		break;

	case AST_EXPR_CAST:
		print_expr_cast(node, pad, stream);
		break;

	case AST_EXPR_BINOP:
		print_expr_binop(node, pad, stream);
		break;

	case AST_EXPR_CALL:
		print_expr_call(node, pad, stream);
		break;

	case AST_EXPR_CATCH:
		print_expr_catch(node, pad, stream);
		break;

	case AST_EXPR_ELEM:
		print_expr_elem(node, pad, stream);
		break;

	case AST_EXPR_TEST_CASES:
		print_expr_test_cases(node, pad, stream);
		break;

	case AST_EXPR_UNARY:
		print_expr_unary(node, pad, stream);
		break;

	case AST_EXPR_ADDROF:
		print_expr_addrof(node, pad, stream);
		break;

	case AST_EXPR_DEREF:
		print_expr_deref(node, pad, stream);
		break;

	case AST_EXPR_NULL:
		break;

	case AST_EXPR_LIT_FN:
		print_expr_lit_fn(node, pad, stream);
		break;

	case AST_EXPR_LIT_FN_GROUP:
		print_expr_lit_fn_group(node, pad, stream);
		break;

	case AST_EXPR_LIT_INT:
		print_expr_lit_int(node, pad, stream);
		break;

	case AST_EXPR_LIT_FLOAT:
		print_expr_lit_float(node, pad, stream);
		break;

	case AST_EXPR_LIT_DOUBLE:
		print_expr_lit_double(node, pad, stream);
		break;

	case AST_EXPR_LIT_CHAR:
		print_expr_lit_char(node, pad, stream);
		break;

	case AST_EXPR_LIT_STRING:
		print_expr_lit_string(node, pad, stream);
		break;

	case AST_EXPR_LIT_BOOL:
		print_expr_lit_bool(node, pad, stream);
		break;

	default:
		break;
	}
}

void ast_printer_run(struct assembly *assembly) {
	for (usize i = 0; i < arrlenu(assembly->units); ++i) {
		struct unit *unit = assembly->units[i];
		print_node(unit->ast, 0, stdout);
	}
	fprintf(stdout, "\n\n");
}
