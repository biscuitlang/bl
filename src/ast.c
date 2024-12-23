// =================================================================================================
// bl
//
// File:   ast.c
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

#include "ast.h"
#include "atomics.h"
#include "stb_ds.h"
#include "tokens.h"

struct ast *
ast_create_node(struct arena *arena, enum ast_kind c, struct token *tok, struct scope *parent_scope) {
	struct ast *node  = arena_alloc(arena);
	node->kind        = c;
	node->owner_scope = parent_scope;
	node->location    = tok ? &tok->location : NULL;
#ifdef BL_DEBUG
	static batomic_s64 serial = 0;
	node->_serial             = batomic_fetch_add_s64(&serial, 1);
#endif
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
	case AST_UNREACHABLE:
		return "Unreachable";
	case AST_DEBUGBREAK:
		return "DebugBreak";
	case AST_CALL_LOC:
		return "CallLocation";
	case AST_STMT_USING:
		return "StmtUsing";
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
	babort("invalid ast node");
}

const char *ast_binop_to_str(enum binop_kind op) {
	switch (op) {
	case BINOP_INVALID:
		return "<invalid>";
	case BINOP_ASSIGN:
		return "=";
	case BINOP_ADD_ASSIGN:
		return "+=";
	case BINOP_SUB_ASSIGN:
		return "-=";
	case BINOP_MUL_ASSIGN:
		return "*=";
	case BINOP_DIV_ASSIGN:
		return "/=";
	case BINOP_MOD_ASSIGN:
		return "%=";
	case BINOP_AND_ASSIGN:
		return "&=";
	case BINOP_OR_ASSIGN:
		return "|=";
	case BINOP_XOR_ASSIGN:
		return "^=";
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

	return "invalid";
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
