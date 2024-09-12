// =================================================================================================
// bl
//
// File:   obj_writer.c
// Author: Martin Dorazil
// Date:   28/02/2018
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
#include "stb_ds.h"

// Emit assembly object file.
void obj_writer_run(struct assembly *assembly) {
	zone();
	runtime_measure_begin(llvm_obj_generation);

	str_buf_t buf = get_tmp_str();

	const struct target *target = assembly->target;
	const char          *name   = target->name;
	blog("out_dir = %.*s", target->out_dir.len, target->out_dir.ptr);
	blog("name = %s", name);

	str_buf_append_fmt(&buf, "{str}/{s}.{s}", target->out_dir, name, OBJ_EXT);
	char *error_msg = NULL;
	if (LLVMTargetMachineEmitToFile(assembly->llvm.TM,
	                                assembly->llvm.module,
	                                str_to_c(buf),
	                                LLVMObjectFile,
	                                &error_msg)) {
		builder_error("Cannot emit object file: %.*s with error: %s", buf.len, buf.ptr, error_msg);
	}
	LLVMDisposeMessage(error_msg);
	put_tmp_str(buf);

	batomic_fetch_add_s32(&assembly->stats.llvm_obj_ms, runtime_measure_end(llvm_obj_generation));
	return_zone();
}
