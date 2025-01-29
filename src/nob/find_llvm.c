const char *LLVM_INCLUDE_DIR = "";
const char *LLVM_LIB_DIR     = "";
const char *LLVM_LIBS        = "";

void find_llvm(void) {

#if _WIN32
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

#else

#define trim_and_dup(sb) temp_sv_to_cstr(sv_trim(sb_to_sv(sb)))
	// try to resolve llvm-config-<VERSION>
	Cmd            cmd = {0};
	String_Builder sb  = {0};
	cmd_append(&cmd, SHELL, "command -v llvm-config-" STR(LLVM_VERSION_MAJOR));
	if (!cmd_run_sync_read_and_reset(&cmd, &sb) || sb.count == 0) {
		nob_log(NOB_ERROR,
		        "Unable to find 'llvm-config-" STR(LLVM_VERSION_MAJOR) "'. LLVM might not be installed on your system, it's missing from PATH or you have incorrect version. Expected LLVM version " STR(LLVM_VERSION_MAJOR) ".");
#ifdef __linux__
		nob_log(NOB_INFO,
		        "You can use the following command sequence to install LLVM. Note that we're not providing this script, and you're executing it at your own risk.\n\n"
		        "\twget https://apt.llvm.org/llvm.sh\n"
		        "\tchmod +x llvm.sh\n"
		        "\tsudo ./llvm.sh " STR(LLVM_VERSION_MAJOR) "\n");
#endif
		exit(1);
	}

	const char *llvm_config = trim_and_dup(sb);
	sb.count                = 0;
	nob_log(NOB_INFO, "LLVM " STR(LLVM_VERSION_MAJOR) " config found: %s", llvm_config);

	// include dir
	cmd_append(&cmd, llvm_config, "--includedir");
	if (!cmd_run_sync_read_and_reset(&cmd, &sb)) exit(1);
	if (sb.count == 0) exit(1);

	LLVM_INCLUDE_DIR = trim_and_dup(sb);
	sb.count         = 0;
	nob_log(NOB_INFO, "LLVM " STR(LLVM_VERSION_MAJOR) " include directory found: %s", LLVM_INCLUDE_DIR);

	// libraries
	cmd_append(&cmd, llvm_config, "--link-static", "--libnames", "core", "support", "X86", "AArch64", "passes");
	if (!cmd_run_sync_read_and_reset(&cmd, &sb)) exit(1);
	if (sb.count == 0) exit(1);

	LLVM_LIBS = trim_and_dup(sb);
	sb.count  = 0;
	// nob_log(NOB_INFO, "LLVM " STR(LLVM_VERSION_MAJOR) " libs: %s", LLVM_LIBS);

	// libdir
	cmd_append(&cmd, llvm_config, "--libdir");
	if (!cmd_run_sync_read_and_reset(&cmd, &sb)) exit(1);
	if (sb.count == 0) exit(1);

	LLVM_LIB_DIR = trim_and_dup(sb);
	sb.count     = 0;
	nob_log(NOB_INFO, "LLVM " STR(LLVM_VERSION_MAJOR) " lib directory found: %s", LLVM_LIB_DIR);

#undef trim_and_dup

#endif
}
