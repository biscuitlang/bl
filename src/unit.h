#ifndef BL_UNIT_H
#define BL_UNIT_H

#include "ast.h"
#include "common.h"
#include "config.h"
#include "scope.h"
#include "tokens.h"

struct token;
struct assembly;

struct unit {
	hash_t        hash;
	struct tokens tokens;
	struct ast   *ast;
	array(struct ast *) ublock_ast;
	struct scope   *parent_scope;
	struct scope   *private_scope;
	struct module  *module;
	str_t           filepath;
	str_t           dirpath;
	str_t           name;
	str_t           filename;
	char           *src;
	struct token   *loaded_from;
	LLVMMetadataRef llvm_file_meta;
	str_buf_t       file_docs_cache;
};

// The inject_to_scope is supposed to be valid scope (parent scope of the #load directive or global scope).
struct unit *unit_new(struct assembly *assembly, const str_t filepath, const str_t name, const hash_t hash, struct token *load_from, struct scope *parent_scope, struct module *module);
void         unit_delete(struct unit *unit);
// Returns single line from the unit source code, len does not count last new line char.
const char *unit_get_src_ln(struct unit *unit, s32 line, long *len);

#endif
