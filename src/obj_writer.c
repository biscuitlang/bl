#include "builder.h"
#include "stb_ds.h"

// Emit assembly object file.
void obj_writer_run(struct assembly *assembly) {
	zone();
	runtime_measure_begin(llvm_obj_generation);

	str_buf_t buf = get_tmp_str();

	const struct target *target = assembly->target;
	const char          *name   = target->name;
	blog("out_dir = " STR_FMT, STR_ARG(target->out_dir));
	blog("name = %s", name);

	str_buf_append_fmt(&buf, "{str}/{s}.{s}", target->out_dir, name, OBJ_EXT);
	char *error_msg = NULL;
	if (LLVMTargetMachineEmitToFile(assembly->llvm.TM,
	                                assembly->llvm.module,
	                                str_buf_to_c(buf),
	                                LLVMObjectFile,
	                                &error_msg)) {
		builder_error("Cannot emit object file: " STR_FMT " with error: %s", STR_ARG(buf), error_msg);
	}
	LLVMDisposeMessage(error_msg);
	put_tmp_str(buf);

	batomic_fetch_add_s32(&assembly->stats.llvm_obj_ms, runtime_measure_end(llvm_obj_generation));
	return_zone();
}
