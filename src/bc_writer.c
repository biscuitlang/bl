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
		builder_error("Cannot open file " STR_FMT "", STR_ARG(export_file));
		put_tmp_str(export_file);
		return_zone();
	}
	fprintf(f, "%s\n", str);
	fclose(f);
	LLVMDisposeMessage(str);
	builder_info("Byte code written into " STR_FMT "", STR_ARG(export_file));
	put_tmp_str(export_file);
	return_zone();
}
