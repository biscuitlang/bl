// =================================================================================================
// bl
//
// File:   bc_writer.c
// Author: Martin Dorazil
// Date:   14.2.18
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

#include "bldebug.h"
#include "builder.h"
#include "stb_ds.h"
#include <string.h>

void bc_writer_run(struct assembly *assembly) {
	zone();
	str_buf_t export_file = get_tmp_str();

	const struct target *target = assembly->target;
	str_buf_append_fmt(&export_file, "{str}/{s}.ll", target->out_dir, target->name);

	char *str = LLVMPrintModuleToString(assembly->llvm.module);
	FILE *f   = fopen(str_buf_to_c(export_file), "w");
	if (f == NULL) {
		builder_error("Cannot open file %.*s", export_file.len, export_file.ptr);
		put_tmp_str(export_file);
		return_zone();
	}
	fprintf(f, "%s\n", str);
	fclose(f);
	LLVMDisposeMessage(str);
	builder_info("Byte code written into %.*s", export_file.len, export_file.ptr);
	put_tmp_str(export_file);
	return_zone();
}
