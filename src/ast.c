#include "ast.h"
#include "atomics.h"
#include "stb_ds.h"
#include "tokens.h"
#include "table.h"
#include "unit.h"

struct ast *ast_create_node(struct arena *arena, enum ast_kind c, struct location *loc, struct scope *parent_scope) {
	struct ast *node      = arena_alloc(arena);
	node->kind            = c;
	node->owner_scope     = parent_scope;
	node->location        = loc;
	static batomic_s64 id = 0;
	node->id              = batomic_fetch_add_s64(&id, 1);
	return node;
}

// public
void ast_arena_init(struct arena *arena, u32 owner_thread_index) {
	arena_init(arena, sizeof(struct ast), alignment_of(struct ast), 8192, owner_thread_index, NULL);
}

void ast_arena_terminate(struct arena *arena) {
	arena_terminate(arena);
}

const char *ast_get_name(const struct ast *n) {
	bassert(n);
	switch (n->kind) {
	case AST_BAD:
		return "Bad";
	case AST_TAG:
		return "Tag";
	case AST_LOAD:
		return "Load";
	case AST_IMPORT:
		return "Import";
	case AST_LINK:
		return "Link";
	case AST_PRIVATE:
		return "Private";
	case AST_MODULE_PRIVATE:
		return "ModulePrivate";
	case AST_PUBLIC:
		return "Public";
	case AST_IDENT:
		return "Ident";
	case AST_UBLOCK:
		return "UBlock";
	case AST_BLOCK:
		return "Block";
	case AST_DOCS:
		return "Docs";
	case AST_REF:
		return "Ref";
	case AST_LIST:
		return "List";
	case AST_UNREACHABLE:
		return "Unreachable";
	case AST_DEBUGBREAK:
		return "DebugBreak";
	case AST_CALL_LOC:
		return "CallLocation";
	case AST_STMT_USING:
		return "StmtUsing";
	case AST_EXPR_ERR:
		return "ExprErr";
	case AST_STMT_RETURN:
		return "StmtReturn";
	case AST_STMT_DEFER:
		return "StmtDefer";
	case AST_STMT_IF:
		return "StmtIf";
	case AST_STMT_LOOP:
		return "StmtLoop";
	case AST_STMT_BREAK:
		return "StmtBreak";
	case AST_STMT_CONTINUE:
		return "StmtContinue";
	case AST_STMT_SWITCH:
		return "StmtSwitch";
	case AST_STMT_CASE:
		return "StmtCase";
	case AST_STMT_ASSIGN:
		return "StmtAssign";
	case AST_DECL_ENTITY:
		return "DeclEntity";
	case AST_DECL_MEMBER:
		return "DeclMember";
	case AST_DECL_ARG:
		return "DeclArg";
	case AST_DECL_VARIANT:
		return "DeclVariant";
	case AST_TYPE_ARR:
		return "TypeArr";
	case AST_TYPE_SLICE:
		return "TypeSlice";
	case AST_TYPE_DYNARR:
		return "TypeDynamicArray";
	case AST_TYPE_FN:
		return "TypeFn";
	case AST_TYPE_STRUCT:
		return n->data.type_strct.is_union ? "TypeUnion" : "TypeStruct";
	case AST_TYPE_ENUM:
		return "TypeEnum";
	case AST_TYPE_PTR:
		return "TypePtr";
	case AST_TYPE_VARGS:
		return "TypeVargs";
	case AST_TYPE_FN_GROUP:
		return "TypeFnGroup";
	case AST_TYPE_POLY:
		return "TypePoly";
	case AST_EXPR_CAST:
		return "ExprCast";
	case AST_EXPR_BINOP:
		return "ExprBinop";
	case AST_EXPR_CALL:
		return "ExprCall";
	case AST_EXPR_CATCH:
		return "ExprCatch";
	case AST_EXPR_ELEM:
		return "ExprElem";
	case AST_EXPR_TEST_CASES:
		return "ExprTestCases";
	case AST_EXPR_TYPE:
		return "ExprType";
	case AST_EXPR_UNARY:
		return "ExprUnary";
	case AST_EXPR_NULL:
		return "ExprNull";
	case AST_EXPR_ADDROF:
		return "ExprAddrOf";
	case AST_EXPR_DEREF:
		return "ExprDeref";
	case AST_EXPR_COMPOUND:
		return "ExprCompound";
	case AST_EXPR_LIT_FN:
		return "ExprLitFn";
	case AST_EXPR_LIT_FN_GROUP:
		return "ExprLitFnGroup";
	case AST_EXPR_LIT_INT:
		return "ExprLitInt";
	case AST_EXPR_LIT_FLOAT:
		return "ExprLitFloat";
	case AST_EXPR_LIT_DOUBLE:
		return "ExprLitDouble";
	case AST_EXPR_LIT_CHAR:
		return "ExprLitChar";
	case AST_EXPR_LIT_STRING:
		return "ExprLitString";
	case AST_EXPR_LIT_BOOL:
		return "ExprLitBool";
	}

	return "<invalid>";
}

const char *ast_binop_to_str(enum binop_kind op) {
	switch (op) {
	case BINOP_INVALID:
		break;
	case BINOP_ADD:
		return "+";
	case BINOP_SUB:
		return "-";
	case BINOP_MUL:
		return "*";
	case BINOP_DIV:
		return "/";
	case BINOP_MOD:
		return "%";
	case BINOP_EQ:
		return "==";
	case BINOP_NEQ:
		return "!=";
	case BINOP_GREATER:
		return ">";
	case BINOP_LESS:
		return "<";
	case BINOP_GREATER_EQ:
		return ">=";
	case BINOP_LESS_EQ:
		return "<=";
	case BINOP_LOGIC_AND:
		return "&&";
	case BINOP_LOGIC_OR:
		return "||";
	case BINOP_AND:
		return "&";
	case BINOP_OR:
		return "|";
	case BINOP_XOR:
		return "^";
	case BINOP_SHR:
		return ">>";
	case BINOP_SHL:
		return "<<";
	}

	return "<invalid>";
}

const char *ast_assign_to_str(enum assign_kind op) {
	switch (op) {
	case ASSIGN_INVALID:
		break;
	case ASSIGN:
		return "=";
	case ASSIGN_ADD:
		return "+=";
	case ASSIGN_SUB:
		return "-=";
	case ASSIGN_MUL:
		return "*=";
	case ASSIGN_DIV:
		return "/=";
	case ASSIGN_MOD:
		return "%=";
	case ASSIGN_AND:
		return "&=";
	case ASSIGN_OR:
		return "|=";
	case ASSIGN_XOR:
		return "^=";
	}

	return "<invalid>";
}

const char *ast_unop_to_str(enum unop_kind op) {
	switch (op) {
	case UNOP_NEG:
		return "-";
	case UNOP_POS:
		return "+";
	case UNOP_NOT:
		return "!";
	case UNOP_BIT_NOT:
		return "~";
	default:
		return "invalid";
	}
}

str_t ast_get_docs(struct unit *unit, struct ast *node) {
	const s32 index = tbl_lookup_index(unit->docs, node->id);
	if (index == -1) return str_empty;
	return unit->docs[index].text;
}

bool ast_binop_is_logic(enum binop_kind op) {
	return op >= BINOP_EQ && op <= BINOP_LOGIC_OR;
}
