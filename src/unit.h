// =================================================================================================
// bl
//
// File:   unit.h
// Author: Martin Dorazil
// Date:   3/1/18
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
	struct scope   *private_scope;
	str_t           filepath;
	char           *filename;
	char           *dirpath;
	char           *name;
	char           *src;
	struct token   *loaded_from;
	LLVMMetadataRef llvm_file_meta;
	str_buf_t       file_docs_cache;
};

hash_t       unit_hash(const char *filepath, struct token *load_from);
struct unit *unit_new(struct assembly *assembly, const char *filepath, struct token *load_from);
void         unit_delete(struct unit *unit);
// Returns single line from the unit source code, len does not count last new line char.
const char *unit_get_src_ln(struct unit *unit, s32 line, long *len);

#endif
