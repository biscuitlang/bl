const char *LLVM_INCLUDE_DIR = "";
const char *LLVM_LIB_DIR     = "";
const char *LLVM_LIBS        = "";

void find_llvm(void) {
	if (!file_exists("./deps/llvm-18.1.8-win64")) {
		nob_log(NOB_INFO, "Unpacking LLVM...");
		Cmd cmd = {0};
		cmd_append(&cmd, "tar", "-xf", "./deps/llvm-" LLVM_VERSION "-win64.zip", "-C", "./deps");
		if (!cmd_run_sync(cmd)) exit(1);
		cmd_free(cmd);
	}

	LLVM_INCLUDE_DIR = "./deps/llvm-" LLVM_VERSION "-win64/include";
	LLVM_LIB_DIR     = "./deps/llvm-" LLVM_VERSION "-win64/lib";
	LLVM_LIBS        = "LLVM.lib";
}