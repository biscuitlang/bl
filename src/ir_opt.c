#include "bldebug.h"
#include "builder.h"

void ir_opt_run(struct assembly *assembly) {
	zone();
	// 2024-08-09 LLVM is slow, so no passes for debug.
	if (assembly->target->opt == ASSEMBLY_OPT_DEBUG) return_zone();

	LLVMModuleRef        llvm_module = assembly->llvm.module;
	LLVMTargetMachineRef llvm_tm     = assembly->llvm.TM;

	str_t opt = opt_to_LLVM_pass_str(assembly->target->opt);

	str_buf_t tmp = get_tmp_str();
	str_buf_append_fmt(&tmp, "default<{str}>", opt);

	LLVMPassBuilderOptionsRef options = LLVMCreatePassBuilderOptions();

	const char  *passes = str_buf_to_c(tmp);
	LLVMErrorRef err    = LLVMRunPasses(llvm_module, passes, llvm_tm, options);
	if (err != LLVMErrorSuccess) {
		char *msg = LLVMGetErrorMessage(err);
		builder_error("LLVM error: %s", msg);
		LLVMDisposeErrorMessage(msg);
	}

	put_tmp_str(tmp);
	return_zone();
}
