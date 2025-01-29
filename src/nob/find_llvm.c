const char *LLVM_VERSION     = "UNKNOWN";
const char *LLVM_INCLUDE_DIR = "";
const char *LLVM_LIB_DIR     = "";
const char *LLVM_LIBS        = "";

void find_llvm(void) {
	nob_log(NOB_INFO, "Looking for LLVM " STR(LLVM_REQUIRED) "...");

#if _WIN32
	nob_log(NOB_INFO, "Using bundled LLVM package.");
	if (!file_exists("./deps/llvm-18.1.8-win64")) {
		nob_log(NOB_INFO, "Unpacking LLVM...");
		Cmd cmd = {0};
		cmd_append(&cmd, "tar", "-xf", "./deps/llvm-" STR(LLVM_REQUIRED) "-win64.zip", "-C", "./deps");
		if (!cmd_run_sync(cmd)) exit(1);
		cmd_free(cmd);
	}

	LLVM_VERSION     = "18.1.8";
	LLVM_INCLUDE_DIR = temp_sprintf("./deps/llvm-%s-win64/include", LLVM_VERSION);
	LLVM_LIB_DIR     = temp_sprintf("./deps/llvm-%s-win64/lib", LLVM_VERSION);
	LLVM_LIBS        = "LLVM.lib";

#else

	// try to resolve llvm-config-<VERSION>
	Cmd            cmd = {0};
	String_Builder sb  = {0};

	const char *llvm_config = "";

#ifdef __APPLE__
	// Try brew on mac...
	cmd_append(&cmd, SHELL, "brew --prefix llvm@" STR(LLVM_REQUIRED) " 2>/dev/null");
	cmd_run_sync_read_and_reset(&cmd, &sb);
	const char *brew_prefix = trim_and_dup(sb);
	sb.count = 0;

	if (strlen(brew_prefix)) {
		nob_log(NOB_INFO, "Brew prefix: '%s'.", brew_prefix);
		cmd_append(&cmd, SHELL, temp_sprintf("command -v %s/bin/llvm-config", brew_prefix));
		cmd_run_sync_read_and_reset(&cmd, &sb);
		llvm_config = trim_and_dup(sb);
		sb.count    = 0;
	}
#endif

	if (!strlen(llvm_config)) {
		cmd_append(&cmd, SHELL, "command -v llvm-config-" STR(LLVM_REQUIRED) " 2>/dev/null");
		cmd_run_sync_read_and_reset(&cmd, &sb);
		llvm_config = trim_and_dup(sb);
		sb.count    = 0;
	}

	if (!strlen(llvm_config)) {
		nob_log(NOB_ERROR,
		        "Unable to find 'llvm-config-" STR(LLVM_REQUIRED) "'. LLVM might not be installed on your system, it's missing from PATH or you have incorrect version. Expected LLVM version " STR(LLVM_REQUIRED) ".");
#ifdef __linux__
		nob_log(NOB_INFO,
		        "Install LLVM development package (such as 'llvm-" STR(LLVM_REQUIRED) "-dev') using your package manager or "
		                                                                              "you can use the following command sequence to install LLVM. Note that we're not providing this script, and you're executing it at your own risk.\n\n"
		                                                                              "\twget https://apt.llvm.org/llvm.sh\n"
		                                                                              "\tchmod +x llvm.sh\n"
		                                                                              "\tsudo ./llvm.sh " STR(LLVM_REQUIRED) "\n");
#elif __APPLE__
		nob_log(NOB_INFO, "Use homebrew package manager to install LLVM 'brew install llvm@"STR(LLVM_REQUIRED)"'.");
#endif
		exit(1);
	}

	nob_log(NOB_INFO, "LLVM " STR(LLVM_REQUIRED) " config found: %s", llvm_config);

	// version
	cmd_append(&cmd, llvm_config, "--version");
	if (!cmd_run_sync_read_and_reset(&cmd, &sb)) exit(1);
	if (sb.count == 0) exit(1);
	LLVM_VERSION = trim_and_dup(sb);
	sb.count     = 0;
	nob_log(NOB_INFO, "Using LLVM %s.", LLVM_VERSION);

	// include dir
	cmd_append(&cmd, llvm_config, "--includedir");
	if (!cmd_run_sync_read_and_reset(&cmd, &sb)) exit(1);
	if (sb.count == 0) exit(1);
	LLVM_INCLUDE_DIR = trim_and_dup(sb);
	sb.count         = 0;
	nob_log(NOB_INFO, "LLVM " STR(LLVM_REQUIRED) " include directory found: %s", LLVM_INCLUDE_DIR);

	// libraries
	cmd_append(&cmd, llvm_config, "--link-static", "--libnames", "core", "support", "X86", "AArch64", "passes");
	if (!cmd_run_sync_read_and_reset(&cmd, &sb)) exit(1);
	if (sb.count == 0) exit(1);
	LLVM_LIBS = trim_and_dup(sb);
	sb.count  = 0;
	// nob_log(NOB_INFO, "LLVM " STR(LLVM_REQUIRED) " libs: %s", LLVM_LIBS);

	// libdir
	cmd_append(&cmd, llvm_config, "--libdir");
	if (!cmd_run_sync_read_and_reset(&cmd, &sb)) exit(1);
	if (sb.count == 0) exit(1);
	LLVM_LIB_DIR = trim_and_dup(sb);
	sb.count     = 0;
	nob_log(NOB_INFO, "LLVM " STR(LLVM_REQUIRED) " lib directory found: %s", LLVM_LIB_DIR);

#endif
}
