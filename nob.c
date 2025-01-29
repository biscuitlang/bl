/*

    We use 'nob.h' "build system" created by Alexey Kutepov (https://github.com/tsoding/nob.h) with some small
    modifications.

    Since build system is written in C you need to compile it first in order to build the compiler.

    Windows:
    - You need x64 Visual Studio development environment loaded in you environment, use "Developer Command Prompt"
      or 'vcvars.bat' to inject environment variables in current shell.
    - Run 'cl nob.c'

    Linux/macOS:
    - Run 'cc nob.c -o nob'


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

// Comment this to disable ANSI color output.
#define NOB_COLORS
// Comment this to enable verbose build.
// #define NOB_NO_ECHO
#define NOB_FORCE_UNIX_PATH

#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "deps/nob.h"

#define STR_HELPER(x)           #x
#define STR(x)                  STR_HELPER(x)
#define VERSION_STRING(a, b, c) STR(a) "." STR(b) "." STR(c)
#define quote(x)                temp_sprintf("\"%s\"", (x))
#define trim_and_dup(sb)        temp_sv_to_cstr(sv_trim(sb_to_sv(sb)))

#ifdef _WIN32
#define SHELL "CMD", "/C"
void lib(const char *dir, const char *libname);
#elif __linux__
#define SHELL "bash", "-c"
void ar(const char *dir, const char *libname);
#else
#error "Unsupported platform."
#endif

#define shell(...) _shell((sizeof((const char *[]){__VA_ARGS__}) / sizeof(const char *)), ((const char *[]){__VA_ARGS__}))
const char *_shell(int argc, const char *argv[]);

// #define run_shell_cmd(c)                 \
// 	do {                                 \
// 		Cmd cmd = {0};                   \
// 		cmd_append(&cmd, SHELL, c);      \
// 		if (!cmd_run_sync(cmd)) exit(1); \
// 		cmd_free(cmd);                   \
// 	} while (0)

#define wait(procs)                                  \
	do {                                             \
		bool success = true;                         \
		for (int i = 0; i < ARRAY_LEN(procs); ++i) { \
			success &= proc_wait(procs[i]);          \
		}                                            \
		if (!success) exit(1);                       \
	} while (0);

int IS_DEBUG = 0;

void print_help(void);
void parse_command_line_arguments(int argc, char *argv[]);

#include "deps/dyncall-1.2/nob.c"
#include "deps/libyaml-0.2.5/nob.c"
#include "src/nob/nob.c"

int main(int argc, char *argv[]) {
	parse_command_line_arguments(argc, argv);
	nob_log(NOB_INFO, "Running in '%s' in '%s' mode.", get_current_dir_temp(), IS_DEBUG ? "DEBUG" : "RELEASE");
	mkdir_if_not_exists(BUILD_DIR);

	setup();

	if (!file_exists(BUILD_DIR "/dyncall/" DYNCALL_LIB)) dyncall();
	if (!file_exists(BUILD_DIR "/libyaml/" YAML_LIB)) libyaml();
	blc();
	finalize();

	nob_log(NOB_INFO, "All files compiled successfully.");
	return 0;
}

void print_help(void) {
	printf("Usage:\n\tbuild [options]\n\n");
	printf("Options:\n");
	printf("\trelease Build in release mode.\n");
	printf("\tdebug   Build in debug mode.\n");
	printf("\tclean   Remove build directory and exit.\n");
	printf("\thelp    Print this help and exit.\n");
}

void parse_command_line_arguments(int argc, char *argv[]) {
	shift_args(&argc, &argv);
	while (argc > 0) {
		char *arg = shift_args(&argc, &argv);
		if (strcmp(arg, "debug") == 0) {
			IS_DEBUG = 1;
		} else if (strcmp(arg, "release") == 0) {
			IS_DEBUG = 0; // by default
		} else if (strcmp(arg, "clean") == 0) {
			cleanup();
			exit(0);
		} else if (strcmp(arg, "help") == 0) {
			print_help();
			exit(0);
		} else {
			nob_log(NOB_ERROR, "Invalid argument '%s'.", arg);
			print_help();
			exit(1);
		}
	}
}

const char *_shell(int argc, const char *argv[]) {
	Cmd            cmd = {0};
	String_Builder sb  = {0};

	cmd_append(&cmd, SHELL);
	for (int i = 0; i < argc; ++i) cmd_append(&cmd, argv[i]);
	if (!cmd_run_sync_read_and_reset(&cmd, &sb)) {
		exit(1);
	}
	if (!sb.count) return "";
	return trim_and_dup(sb);
}

#ifdef _WIN32

void lib(const char *dir, const char *libname) {
	File_Paths files = {0};
	nob_read_entire_dir(dir, &files);

	Cmd cmd = {0};
	cmd_append(&cmd, "lib", "-nologo", temp_sprintf("-OUT:\"%s/%s\"", dir, libname));
	for (int i = 0; i < files.count; ++i) {
		if (ends_with(files.items[i], ".obj")) cmd_append(&cmd, temp_sprintf("%s/%s", dir, files.items[i]));
	}
	if (!cmd_run_sync_and_reset(&cmd)) exit(1);
}

#else

void ar(const char *dir, const char *libname) {
	File_Paths files = {0};
	nob_read_entire_dir(dir, &files);

	Cmd cmd = {0};
	cmd_append(&cmd, "ar", "rcs", temp_sprintf("%s/%s", dir, libname));
	for (int i = 0; i < files.count; ++i) {
		if (ends_with(files.items[i], ".o")) cmd_append(&cmd, temp_sprintf("%s/%s", dir, files.items[i]));
	}
	if (!cmd_run_sync_and_reset(&cmd)) exit(1);
}

#endif

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
