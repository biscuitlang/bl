/*

    We use 'nob.h' "build system" created by Alexey Kutepov (https://github.com/tsoding/nob.h) with some small
    modifications.

    Since the build system is written in C you need to compile it first in order to build the compiler.

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
    - 2025-01-29: Add assert mode switch argument.
    - 2025-01-27: Missing Tracy support.
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
#define NOB_NO_ECHO

#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "deps/nob.h"

#define STR_HELPER(x)           #x
#define STR(x)                  STR_HELPER(x)
#define VERSION_STRING(a, b, c) STR(a) "." STR(b) "." STR(c)
#define quote(x)                temp_sprintf("\"%s\"", (x))
#define trim_and_dup(sb)        temp_sv_to_cstr(sv_trim(sb_to_sv(sb)))
#define strok(x)                (x && x[0] != '\0')

#ifdef _WIN32
#define SHELL        "CMD", "/C"
#define EXEC_NAME(n) n ".exe"
void lib(const char *dir, const char *libname);
#elif __linux__
#define SHELL        "bash", "-c"
void ar(const char *dir, const char *libname);
#define EXEC_NAME(n) n
#elif __APPLE__
#if !defined(__aarch64__) && !defined(__arm64__)
#error "Old intel based Apple CPUs are not supported right now."
#endif
#define SHELL        "zsh", "-c"
void ar(const char *dir, const char *libname);
#define EXEC_NAME(n) n
#else
#error "Unsupported platform."
#endif

#define shell(...) _shell((sizeof((const char *[]){__VA_ARGS__}) / sizeof(const char *)), ((const char *[]){__VA_ARGS__}))
const char *_shell(int argc, const char *argv[]);

#define wait(procs)                                  \
	do {                                             \
		bool success = true;                         \
		for (int i = 0; i < ARRAY_LEN(procs); ++i) { \
			success &= proc_wait(procs[i]);          \
		}                                            \
		if (!success) exit(1);                       \
	} while (0);

bool IS_DEBUG      = false;
bool ASSERT_ENABLE = false;

#define TARGET_BLC     1 << 0
#define TARGET_RUNTIME 1 << 1
#define TARGET_TESTS   1 << 2
#define TARGET_DOCS    1 << 3

int TARGET = 0;

void print_help(void);
void parse_command_line_arguments(int argc, char *argv[]);
void check_compiler(void);
void run_tests(void);
void docs(void);

#include "deps/dyncall-1.2/nob.c"
#include "deps/libyaml-0.2.5/nob.c"
#include "src/nob/nob.c"

int main(int argc, char *argv[]) {
	parse_command_line_arguments(argc, argv);
	nob_log(NOB_INFO, "Running in '%s'.", get_current_dir_temp());

	check_compiler();
	if (TARGET & TARGET_BLC) {
		mkdir_if_not_exists(BUILD_DIR);
		mkdir_if_not_exists(BIN_DIR);

		setup();
		if (!file_exists(BUILD_DIR "/dyncall/" DYNCALL_LIB)) dyncall();
		if (!file_exists(BUILD_DIR "/libyaml/" YAML_LIB)) libyaml();
		blc();
	}
	if (TARGET & TARGET_RUNTIME) blc_runtime();
	if (TARGET & TARGET_BLC) finalize();
	if (TARGET & TARGET_TESTS) run_tests();
	if (TARGET & TARGET_DOCS) docs();

	nob_log(NOB_INFO, "All files compiled successfully.");
	return 0;
}

void print_help(void) {
	printf("Usage:\n\tbuild [options]\n\n");
	printf("Options:\n");
	printf("\tall        Build everything and run unit tests.\n");
	printf("\tassert     Build bl compiler in release mode with asserts enabled.\n");
	printf("\tbuild-all  Build everything.\n");
	printf("\tclean      Remove build directory and exit.\n");
	printf("\tdebug      Build bl compiler in debug mode.\n");
	printf("\tdocs       Build bl documentation.\n");
	printf("\thelp       Print this help and exit.\n");
	printf("\trelease    Build bl compiler in release mode (default).\n");
	printf("\truntime    Build compiler runtime.\n");
	printf("\ttest       Run tests.\n");
}

void parse_command_line_arguments(int argc, char *argv[]) {
	shift_args(&argc, &argv);
	// In case no arguments were specified build te compiler by default in release mode.
	while (argc > 0) {
		char *arg = shift_args(&argc, &argv);
		if (strcmp(arg, "debug") == 0) {
			IS_DEBUG = true;
			TARGET |= TARGET_BLC;
		} else if (strcmp(arg, "release") == 0) {
			IS_DEBUG      = false;
			ASSERT_ENABLE = false;
			TARGET |= TARGET_BLC;
		} else if (strcmp(arg, "assert") == 0) {
			IS_DEBUG      = false;
			ASSERT_ENABLE = true;
			TARGET |= TARGET_BLC;
		} else if (strcmp(arg, "runtime") == 0) {
			TARGET |= TARGET_RUNTIME;
		} else if (strcmp(arg, "test") == 0) {
			TARGET |= TARGET_TESTS;
		} else if (strcmp(arg, "docs") == 0) {
			TARGET |= TARGET_DOCS;
		} else if (strcmp(arg, "all") == 0) {
			TARGET |= TARGET_RUNTIME | TARGET_BLC | TARGET_DOCS | TARGET_TESTS;
		} else if (strcmp(arg, "build-all") == 0) {
			TARGET |= TARGET_RUNTIME | TARGET_BLC | TARGET_DOCS;
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

	if (TARGET == 0) TARGET |= TARGET_BLC;
	if (IS_DEBUG) ASSERT_ENABLE = true;
}

const char *_shell(int argc, const char *argv[]) {
	Cmd            cmd = {0};
	String_Builder sb  = {0};

	cmd_append(&cmd, SHELL);
	for (int i = 0; i < argc; ++i)
		cmd_append(&cmd, argv[i]);
	if (!cmd_run_sync_read_and_reset(&cmd, &sb)) {
		return NULL;
	}
	if (!sb.count) return "";
	return trim_and_dup(sb);
}

void check_compiler(void) {
#ifdef _WIN32
	const char *cl = shell("where", "cl");
	if (!strok(cl)) {
		nob_log(NOB_ERROR, "Cannot find 'cl.exe' compiler. Have you forgot to run 'vcvars64.bat' to inject development environment?");
		exit(1);
	}
#endif
}

void run_tests(void) {
	nob_log(NOB_INFO, "Running tests.");
	Cmd cmd = {0};
	cmd_append(&cmd, BIN_DIR "/" EXEC_NAME("blc"), "-run", "doctor.bl");
	if (!nob_cmd_run_sync_and_reset(&cmd)) exit(1);
}

void docs(void) {
	const char *root = get_current_dir_temp();
	nob_set_current_dir("./docs");

	nob_log(NOB_INFO, "Generate documentation.");
	Cmd cmd = {0};
	cmd_append(&cmd, "../" BIN_DIR "/" EXEC_NAME("blc"), "-build", "docs.bl");
	if (!nob_cmd_run_sync_and_reset(&cmd)) exit(1);

	cmd_append(&cmd, "./" EXEC_NAME("docs"));
	if (!nob_cmd_run_sync_and_reset(&cmd)) exit(1);

	nob_set_current_dir(root);
	nob_log(NOB_INFO, "Documentation generated './docs/side/index.html'");
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
