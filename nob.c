/*

    We use 'nob.h' "build system" created by Alexey Kutepov (https://github.com/tsoding/nob.h) with some small
    modifications.

    Since build system is written in C you need to compile it first in order to build the compiler.

    Windows:
    - You need x64 Visual Studio development environment loaded in you environment, use "Developer Command Prompt"
      or 'vcvars.bat' to inject environment variables in current shell.
    - Run 'cl build.c'

    Linux/macOS:
    - Run 'cc build.c -o build'


    Some notes:
    We don't care about memory management here too much, despite the fact the nob provides propper ways to handle
    it. Since this program is supposed to act, more or less, as an one-shot script, we leave memory cleanup on the
    operating system.

    TODO:
    - 2025-01-27: Missing Tracy support.
    - 2025-01-27: Missing build of dlib runtime used by compiler.
    - 2025-01-27: There is no way to "install" results.

*/

//
// General options
//

#ifdef _WIN32
#define BL_RPMALLOC_ENABLE 1
#define BL_SIMD_ENABLE     1
#else
#define BL_RPMALLOC_ENABLE 0
#define BL_SIMD_ENABLE     0
#endif

#define BL_TRACY_ENABLE            0 // not used yet
#define BL_EXPORT_COMPILE_COMMANDS 1

#define BUILD_DIR "./build"
#define BIN_DIR   "./bin"

#define NOB_COLORS
#define NOB_FORCE_UNIX_PATH
#define NOB_NO_ECHO

#define BL_VERSION_MAJOR 0
#define BL_VERSION_MINOR 13
#define BL_VERSION_PATCH 0

#define YAML_VERSION_MAJOR 0
#define YAML_VERSION_MINOR 2
#define YAML_VERSION_PATCH 5

#define LLVM_VERSION_MAJOR 18
#define LLVM_VERSION_MINOR 1
#define LLVM_VERSION_PATCH 8

#define BL_VERSION       VERSION_STRING(BL_VERSION_MAJOR, BL_VERSION_MINOR, BL_VERSION_PATCH)
#define YAML_VERSION     VERSION_STRING(YAML_VERSION_MAJOR, YAML_VERSION_MINOR, YAML_VERSION_PATCH)
#define LLVM_VERSION     VERSION_STRING(LLVM_VERSION_MAJOR, LLVM_VERSION_MINOR, LLVM_VERSION_PATCH)
#define TRACY_VERSION    "0.9.1"
#define RPMALLOC_VERSION "1.4.4"
#define DYNCALL_VERSION  "1.2"
#define VSWHERE_VERSION  "2.8.4"

#include "src/nob/common.c"

#ifdef _WIN32
#elif __linux__
#include "src/nob/linux.c"
#else
#error "Unsupported platform."
#endif

void copy_results(void);
void print_help(void);
void cmd_append_bl_includes(Cmd *cmd);
void cmd_append_bl_flags(Cmd *cmd);

int main(int argc, char *argv[]) {
	NOB_GO_REBUILD_URSELF(argc, argv);
	parse_command_line_arguments(argc, argv);

	nob_log(NOB_INFO, "Running in '%s' in '%s' mode.", get_current_dir_temp(), IS_DEBUG ? "DEBUG" : "RELEASE");
	mkdir_if_not_exists(BUILD_DIR);

	find_llvm();
	build_dyncall();
	build_libyaml();
	build_blc();
	finalize();

	/*
	if (!file_exists(BIN_DIR "/bl-lld.exe")) {
	    copy_file("./deps/lld.exe", BIN_DIR "/bl-lld.exe");
	}
	if (!file_exists(BIN_DIR "/vswhere.exe")) {
	    copy_file("./deps/vswhere-" VSWHERE_VERSION "/vswhere.exe", BIN_DIR "/vswhere.exe");
	}
	*/

	nob_log(NOB_INFO, "All files compiled successfully.");
	return 0;
}

// void cmd_append_bl_includes(Cmd *cmd) {
// 	cmd_append(cmd, "-I./deps/dyncall-" DYNCALL_VERSION "/dyncall");
// 	cmd_append(cmd, "-I./deps/dyncall-" DYNCALL_VERSION "/dynload");
// 	cmd_append(cmd, "-I./deps/dyncall-" DYNCALL_VERSION "/dyncallback");
// 	cmd_append(cmd, "-I./deps/libyaml-" YAML_VERSION "/include");
// 	cmd_append(cmd, "-I./deps/tracy-" TRACY_VERSION "/public/tracy");
// 	cmd_append(cmd, "-I./deps/rpmalloc-" RPMALLOC_VERSION "/rpmalloc");
// 	cmd_append(cmd, temp_sprintf("-I%s", LLVM_INCLUDE_DIR));
// }

// void cmd_append_bl_flags(Cmd *cmd) {
// 	cmd_append(cmd,
// 	           "-DBL_VERSION_MAJOR=" STR(BL_VERSION_MAJOR),
// 	           "-DBL_VERSION_MINOR=" STR(BL_VERSION_MINOR),
// 	           "-DBL_VERSION_PATCH=" STR(BL_VERSION_PATCH),
// 	           "-DYAML_DECLARE_STATIC");
// 	if (IS_DEBUG) {
// 		cmd_append(cmd, BASE_FLAGS, BASE_FLAGS_DEBUG, "-DBL_DEBUG", "-DBL_ASSERT_ENABLE=1");
// 	} else {
// 		cmd_append(cmd, BASE_FLAGS, BASE_FLAGS_RELEASE);
// 	}
// 	if (BL_SIMD_ENABLE) {
// 		cmd_append(cmd, "-DBL_USE_SIMD", "/arch:AVX");
// 	}
// 	if (BL_RPMALLOC_ENABLE) {
// 		cmd_append(cmd, "-DBL_RPMALLOC_ENABLE=1");
// 	}
// }
