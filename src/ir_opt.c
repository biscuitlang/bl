// =================================================================================================
// bl
//
// File:   ir_opt.c
// Author: Martin Dorazil
// Date:   5.2.19
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

void ir_opt_run(struct assembly *assembly) {
	zone();
	LLVMModuleRef        llvm_module = assembly->llvm.module;
	LLVMTargetMachineRef llvm_tm     = assembly->llvm.TM;

	str_t opt = opt_to_LLVM_pass_str(assembly->target->opt);

	str_buf_t tmp = get_tmp_str();
	str_buf_append_fmt(&tmp, "default<{str}>", opt);

	LLVMPassBuilderOptionsRef options = LLVMCreatePassBuilderOptions();

	const char  *passes = str_to_c(tmp);
	LLVMErrorRef err    = LLVMRunPasses(llvm_module, passes, llvm_tm, options);
	if (err != LLVMErrorSuccess) {
		char *msg = LLVMGetErrorMessage(err);
		builder_error("LLVM error: %s", msg);
		LLVMDisposeErrorMessage(msg);
	}

	put_tmp_str(tmp);
	return_zone();
}
