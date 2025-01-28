/*

    We use 'nob.h' "build system" created by Alexey Kutepov (https://github.com/tsoding/nob.h) with some small
    modifications.

    Since build system is written in C you need to compile it first in order to build the compiler.

    Windows:
    - You need x64 Visual Studio development environment loaded in you environment, use "Developer Command Prompt"
      or 'vcvars.bat' to inject environment variables in current shell.
    - Run 'cl build.c'

    Linux/macOS:
    - Run 'cc build.c'


    Some notes:
    We don't care about memory management here too much, despite the fact the nob provides propper ways to handle
    it. Since this program is supposed to act, more or less, as an one-shot script, we leave memory cleanup on the
    operating system.

    TODO:
    - 2025-01-27: Missing Tracy support.
    - 2025-01-27: Missing build of dlib runtime used by compiler.
    - 2025-01-27: There is no way to "install" results.

*/

#define NOB_COLORS
#define NOB_FORCE_UNIX_PATH
#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "deps/nob.h"

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define VERSION_STRING(a, b, c) STR(a) "." STR(b) "." STR(c)
#define quote(x) nob_temp_sprintf("\"%s\"", (x))

#define COMPILER "CL"
#define ASSEMBLER "ml64"
#define BUILD_DIR "./build"
#define BIN_DIR "./bin"

#define BL_RPMALLOC_ENABLE 1
#define BL_TRACY_ENABLE 0 // not used yet
#define BL_SIMD_ENABLE 1

#define BL_VERSION_MAJOR 0
#define BL_VERSION_MINOR 13
#define BL_VERSION_PATCH 0

#define YAML_VERSION_MAJOR 0
#define YAML_VERSION_MINOR 2
#define YAML_VERSION_PATCH 5

#define BL_VERSION VERSION_STRING(BL_VERSION_MAJOR, BL_VERSION_MINOR, BL_VERSION_PATCH)
#define YAML_VERSION VERSION_STRING(YAML_VERSION_MAJOR, YAML_VERSION_MINOR, YAML_VERSION_PATCH)
#define TRACY_VERSION "0.9.1"
#define LLVM_VERSION "18.1.8"
#define DYNCALL_VERSION "1.2"
#define RPMALLOC_VERSION "1.4.4"
#define VSWHERE_VERSION "2.8.4"

Cmd BASE_FLAGS;
Cmd BASE_FLAGS_RELEASE;
Cmd BASE_FLAGS_DEBUG;

int IS_DEBUG = 0;

#define run_shell_cmd(...)                          \
	do {                                            \
		Cmd cmd = {0};                              \
		cmd_append(&cmd, "CMD", "/C", __VA_ARGS__); \
		if (!cmd_run_sync(cmd)) exit(1);            \
		cmd_free(cmd);                              \
	} while (0)

// Include other build files here...
#include "deps/dyncall-1.2/build.c"
#include "deps/libyaml-0.2.5/build.c"

void build_blc(void);
void copy_results(void);
void print_help(void);
void cmd_append_bl_includes(Cmd *cmd);
void cmd_append_bl_flags(Cmd *cmd);

int main(int argc, char *argv[]) {
	shift_args(&argc, &argv);
	while (argc > 0) {
		char *arg = shift_args(&argc, &argv);
		if (strcmp(arg, "debug") == 0) {
			IS_DEBUG = 1;
		} else if (strcmp(arg, "release") == 0) {
			IS_DEBUG = 0; // by default
		} else if (strcmp(arg, "clean") == 0) {
			run_shell_cmd("RD", "/S", "/Q", quote(BUILD_DIR));
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

	nob_log(NOB_INFO, "Running in '%s' in '%s' mode.", get_current_dir_temp(), IS_DEBUG ? "DEBUG" : "RELEASE");
	mkdir_if_not_exists(BUILD_DIR);

	cmd_append(&BASE_FLAGS, "-nologo", "-D_WIN32", "-D_WINDOWS", "-DNOMINMAX", "-D_HAS_EXCEPTIONS=0", "-GF", "-MD");
	cmd_append(&BASE_FLAGS_RELEASE, "-O2", "-Oi", "-DNDEBUG", "-GL");
	cmd_append(&BASE_FLAGS_DEBUG, "-Od", "-Zi", "-FS");

	nob_log(NOB_INFO, "Checking dependencies...");
	if (!file_exists(BUILD_DIR "/dyncall/dyncall.lib")) {
		build_dyncall();
	}

	if (!file_exists(BUILD_DIR "/libyaml/yaml.lib")) {
		build_libyaml();
	}

	if (!file_exists("./deps/llvm-18.1.8-win64")) {
		nob_log(NOB_INFO, "Unpacking LLVM...");
		Cmd cmd = {0};
		cmd_append(&cmd, "tar", "-xf", "./deps/llvm-" LLVM_VERSION "-win64.zip", "-C", "./deps");
		if (!cmd_run_sync(cmd)) exit(1);
		cmd_free(cmd);
	}

	build_blc();

	if (!file_exists(BIN_DIR "/bl-lld.exe")) {
		copy_file("./deps/lld.exe", BIN_DIR "/bl-lld.exe");
	}
	if (!file_exists(BIN_DIR "/vswhere.exe")) {
		copy_file("./deps/vswhere-" VSWHERE_VERSION "/vswhere.exe", BIN_DIR "/vswhere.exe");
	}

	nob_log(NOB_INFO, "All files compiled successfully.");
	return 0;
}

void build_blc(void) {
	const char *src[] = {
		"./src/arena.c",
		"./src/asm_writer.c",
		"./src/assembly.c",
		"./src/ast_printer.c",
		"./src/ast.c",
		"./src/bc_writer.c",
		"./src/bldebug.c",
		"./src/blmemory.c",
		"./src/build_api.c",
		"./src/builder.c",
		"./src/common.c",
		"./src/conf.c",
		"./src/docs.c",
		"./src/file_loader.c",
		"./src/intrinsic.c",
		"./src/ir_opt.c",
		"./src/ir.c",
		"./src/lexer.c",
		"./src/linker.c",
		"./src/lld_ld.c",
		"./src/lld_link.c",
		"./src/llvm_api.cpp",
		"./src/main.c",
		"./src/mir_printer.c",
		"./src/mir_writer.c",
		"./src/mir.c",
		"./src/native_bin.c",
		"./src/obj_writer.c",
		"./src/parser.c",
		"./src/scope_printer.c",
		"./src/scope.c",
		"./src/setup.c",
		"./src/table.c",
		"./src/threading.c",
		"./src/tinycthread.c",
		"./src/token_printer.c",
		"./src/tokens.c",
		"./src/unit.c",
		"./src/vm_runner.c",
		"./src/vm.c",
		"./src/vmdbg.c",
		"./src/x86_64.c",
#if BL_RPMALLOC_ENABLE
		"./deps/rpmalloc-" RPMALLOC_VERSION "/rpmalloc/rpmalloc.c",
#endif
	};

	const int src_num = ARRAYSIZE(src);

	Cmd  cmd = {0};
	Proc procs[ARRAYSIZE(src)];

	for (int i = 0; i < src_num; ++i) {
		cmd_append(&cmd, COMPILER, "-c", src[i]);

		// Include paths
		cmd_append_bl_includes(&cmd);

		// Flags
		cmd_extend(&cmd, &BASE_FLAGS);
		cmd_append_bl_flags(&cmd);

		if (ends_with(src[i], ".cpp")) {
			cmd_append(&cmd, "-std:c++17");
		} else {
			cmd_append(&cmd, "-std:c11");
		}

		cmd_append(&cmd, "-Fo\"" BUILD_DIR "/\"");

		procs[i] = nob_cmd_run_async_and_reset(&cmd);
	}

	bool success = true;
	for (int i = 0; i < src_num; ++i) {
		success &= proc_wait(procs[i]);
	}
	if (!success) exit(1);

	nob_log(NOB_INFO, "Linking...");
	{
		File_Paths files = {0};
		nob_read_entire_dir(BUILD_DIR, &files);

		cmd_append(&cmd, COMPILER);
		cmd_extend(&cmd, &BASE_FLAGS);
		if (IS_DEBUG) {
			cmd_extend(&cmd, &BASE_FLAGS_DEBUG);
		} else {
			cmd_extend(&cmd, &BASE_FLAGS_RELEASE);
		}

		for (int i = 0; i < files.count; ++i) {
			if (ends_with(files.items[i], ".obj")) cmd_append(&cmd, temp_sprintf(BUILD_DIR "/%s", files.items[i]));
		}

		cmd_append(&cmd, "-link", "-nologo", "-OUT:\"" BIN_DIR "/blc.exe\"", "-incremental:no", "-opt:ref", "-subsystem:console", "-NODEFAULTLIB:MSVCRTD.lib");
		cmd_append(&cmd, "./deps/llvm-" LLVM_VERSION "-win64/lib/LLVM.lib", BUILD_DIR "/libyaml/yaml.lib", BUILD_DIR "/dyncall/dyncall.lib", "kernel32.lib", "Shlwapi.lib", "Ws2_32.lib", "dbghelp.lib");

		if (!cmd_run_sync_and_reset(&cmd)) exit(1);
	}

	run_shell_cmd("DEL", "/Q", "/F", quote(BUILD_DIR "\\*.obj"));

	nob_log(NOB_INFO, "Generate compilation database.");
	String_Builder sb = {0};
	sb_append_cstr(&sb, "[\n");
	for (int i = 0; i < src_num; ++i) {
		cmd_append(&cmd, COMPILER);
		cmd_append(&cmd, src[i]);
		cmd_append_bl_flags(&cmd);
		if (ends_with(src[i], ".cpp")) {
			cmd_append(&cmd, "-std:c++17");
		} else {
			cmd_append(&cmd, "-std:c11");
		}
		cmd_append_bl_includes(&cmd);

		sb_append_cstr(&sb, "{\n");
		sb_append_cstr(&sb, temp_sprintf("\"directory\":\"%s\",\n", get_current_dir_temp()));
		sb_append_cstr(&sb, "\"command\":\"");
		cmd_render(cmd, &sb);
		sb_append_cstr(&sb, "\",\n");
		sb_append_cstr(&sb, temp_sprintf("\"file\":\"%s\"\n", src[i]));
		if (i + 1 < src_num) {
			sb_append_cstr(&sb, "},\n");
		} else {
			sb_append_cstr(&sb, "}\n");
		}
		cmd.count = 0;
	}
	sb_append_cstr(&sb, "]\n");
	write_entire_file("compile_commands.json", sb.items, sb.count);
}

void print_help(void) {
	printf("Usage:\n\tbuild [options]\n\n");
	printf("Options:\n");
	printf("\trelease Build in release mode.\n");
	printf("\tdebug   Build in debug mode.\n");
	printf("\tclean   Remove build directory and exit.\n");
	printf("\thelp    Print this help and exit.\n");
}

void cmd_append_bl_includes(Cmd *cmd) {
	cmd_append(cmd, "-I./deps/dyncall-" DYNCALL_VERSION "/dyncall");
	cmd_append(cmd, "-I./deps/dyncall-" DYNCALL_VERSION "/dynload");
	cmd_append(cmd, "-I./deps/dyncall-" DYNCALL_VERSION "/dyncallback");
	cmd_append(cmd, "-I./deps/libyaml-" YAML_VERSION "/include");
	cmd_append(cmd, "-I./deps/tracy-" TRACY_VERSION "/public/tracy");
	cmd_append(cmd, "-I./deps/rpmalloc-" RPMALLOC_VERSION "/rpmalloc");
	cmd_append(cmd, "-I./deps/llvm-" LLVM_VERSION "-win64/include");
}

void cmd_append_bl_flags(Cmd *cmd) {
	cmd_append(cmd,
	           "-DBL_VERSION_MAJOR=" STR(BL_VERSION_MAJOR),
	           "-DBL_VERSION_MINOR=" STR(BL_VERSION_MINOR),
	           "-DBL_VERSION_PATCH=" STR(BL_VERSION_PATCH),
	           "-DYAML_DECLARE_STATIC");
	if (IS_DEBUG) {
		cmd_extend(cmd, &BASE_FLAGS_DEBUG);
		cmd_append(cmd, "-DBL_DEBUG", "-DBL_ASSERT_ENABLE=1");
	} else {
		cmd_extend(cmd, &BASE_FLAGS_RELEASE);
	}
	if (BL_SIMD_ENABLE) {
		cmd_append(cmd, "-DBL_USE_SIMD", "/arch:AVX");
	}
	if (BL_RPMALLOC_ENABLE) {
		cmd_append(cmd, "-DBL_RPMALLOC_ENABLE=1");
	}
}
