#define BL_VERSION_MAJOR 0
#define BL_VERSION_MINOR 13
#define BL_VERSION_PATCH 0

#define LLVM_VERSION_MAJOR 18
#define LLVM_VERSION_MINOR 1
#define LLVM_VERSION_PATCH 8

#define BL_VERSION       VERSION_STRING(BL_VERSION_MAJOR, BL_VERSION_MINOR, BL_VERSION_PATCH)
#define LLVM_VERSION     VERSION_STRING(LLVM_VERSION_MAJOR, LLVM_VERSION_MINOR, LLVM_VERSION_PATCH)
#define TRACY_VERSION    "0.9.1"
#define RPMALLOC_VERSION "1.4.4"
#define VSWHERE_VERSION  "2.8.4"

#include "find_llvm.c"

void blc(void) {
	nob_log(NOB_INFO, "Compiling blc-" BL_VERSION ".");
	mkdir_if_not_exists(BUILD_DIR);

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

	const int src_num = ARRAY_LEN(src);

	Cmd include_paths = {0};
	cmd_append(&include_paths, "-I./deps/dyncall-" DYNCALL_VERSION "/dyncall");
	cmd_append(&include_paths, "-I./deps/dyncall-" DYNCALL_VERSION "/dynload");
	cmd_append(&include_paths, "-I./deps/dyncall-" DYNCALL_VERSION "/dyncallback");
	cmd_append(&include_paths, "-I./deps/libyaml-" YAML_VERSION "/include");
	cmd_append(&include_paths, "-I./deps/tracy-" TRACY_VERSION "/public/tracy");
	cmd_append(&include_paths, "-I./deps/rpmalloc-" RPMALLOC_VERSION "/rpmalloc");
	cmd_append(&include_paths, temp_sprintf("-I%s", LLVM_INCLUDE_DIR));

#ifdef _WIN32

	Cmd  cmd = {0};
	Proc procs[ARRAY_LEN(src)];

	for (int i = 0; i < src_num; ++i) {
		const bool is_cxx = ends_with(src[i], ".cpp");
		cmd_append(&cmd, "cl", "-nologo", "-c", src[i]);
		cmd_extend(&cmd, &include_paths);
		cmd_append(&cmd,
		           "-DBL_VERSION_MAJOR=" STR(BL_VERSION_MAJOR),
		           "-DBL_VERSION_MINOR=" STR(BL_VERSION_MINOR),
		           "-DBL_VERSION_PATCH=" STR(BL_VERSION_PATCH),
		           "-DYAML_DECLARE_STATIC");
		cmd_append(&cmd, "-D_WIN32", "-D_WINDOWS", "-DNOMINMAX", "-D_HAS_EXCEPTIONS=0", "-GF", "-MD");
		if (IS_DEBUG) {
			cmd_append(&cmd, "-Od", "-Zi", "-DBL_DEBUG", "-DBL_ASSERT_ENABLE=1");
		} else {
			cmd_append(&cmd, "-O2", "-Oi", "-DNDEBUG", "-GL");
		}
		if (BL_SIMD_ENABLE) cmd_append(&cmd, "-DBL_USE_SIMD", "-arch:AVX");
		if (BL_RPMALLOC_ENABLE) cmd_append(&cmd, "-DBL_RPMALLOC_ENABLE=1");
		cmd_append(&cmd, is_cxx ? "-std:c++17" : "-std:c11");
		cmd_append(&cmd, "-Fo\"" BUILD_DIR "/\"");
		procs[i] = nob_cmd_run_async_and_reset(&cmd);
	}
	wait(procs);

	nob_log(NOB_INFO, "Linking blc-" BL_VERSION ".");
	{
		File_Paths files = {0};
		nob_read_entire_dir(BUILD_DIR, &files);

		cmd_append(&cmd, "cl", "-nologo");
		cmd_append(&cmd, "-D_WIN32", "-D_WINDOWS", "-DNOMINMAX", "-D_HAS_EXCEPTIONS=0", "-GF", "-MD");
		if (IS_DEBUG) {
			cmd_append(&cmd, "-Od", "-Zi", "-DBL_DEBUG", "-DBL_ASSERT_ENABLE=1");
		} else {
			cmd_append(&cmd, "-O2", "-Oi", "-DNDEBUG", "-GL");
		}

		for (int i = 0; i < files.count; ++i) {
			if (ends_with(files.items[i], ".obj")) cmd_append(&cmd, temp_sprintf(BUILD_DIR "/%s", files.items[i]));
		}

		String_View libs = nob_sv_from_cstr(LLVM_LIBS);
		while (libs.count) {
			String_View lib = nob_sv_chop_by_delim(&libs, ' ');
			if (lib.count)
				cmd_append(&cmd, temp_sprintf("%s/" SV_Fmt, LLVM_LIB_DIR, SV_Arg(lib)));
		}

		cmd_append(&cmd, "-link", "-LTCG", "-incremental:no", "-opt:ref", "-subsystem:console", "-NODEFAULTLIB:MSVCRTD.lib");

		cmd_append(&cmd,
		           BUILD_DIR "/libyaml/libyaml.lib",
		           BUILD_DIR "/dyncall/dyncall.lib",
		           "kernel32.lib",
		           "Shlwapi.lib",
		           "Ws2_32.lib",
		           "dbghelp.lib");

		cmd_append(&cmd, "-OUT:\"" BIN_DIR "/blc.exe\"");

		if (!cmd_run_sync_and_reset(&cmd)) exit(1);
	}

	run_shell_cmd("DEL", "/Q", "/F", quote(BUILD_DIR "\\*.obj"));

#elif __linux__

	Cmd  cmd = {0};
	Proc procs[ARRAY_LEN(src)];

	for (int i = 0; i < src_num; ++i) {
		const bool is_cxx = ends_with(src[i], ".cpp");
		cmd_append(&cmd, is_cxx ? "c++" : "cc", "-c", src[i]);
		cmd_extend(&cmd, &include_paths);
		cmd_append(&cmd,
		           "-DBL_VERSION_MAJOR=" STR(BL_VERSION_MAJOR),
		           "-DBL_VERSION_MINOR=" STR(BL_VERSION_MINOR),
		           "-DBL_VERSION_PATCH=" STR(BL_VERSION_PATCH),
		           "-DYAML_DECLARE_STATIC");
		if (IS_DEBUG) {
			abort();
			cmd_append(&cmd,
			// cmd_append(cmd, "-D_GNU_SOURCE", "-rdynamic", "-DBL_DEBUG", "-DBL_ASSERT_ENABLE=1");
		} else {
			cmd_append(&cmd, "-D_GNU_SOURCE", "-rdynamic", "-O3", "-DNDEBUG");
		}
		if (BL_SIMD_ENABLE) nob_log(NOB_WARNING, "BL_SIMD_ENABLE not supported on this platform.");
		if (BL_RPMALLOC_ENABLE) cmd_append(&cmd, "-DBL_RPMALLOC_ENABLE=1");
		cmd_append(&cmd, is_cxx ? "-std=c++17" : "-std=gnu11");
		cmd_append(&cmd, "-o", temp_sprintf(BUILD_DIR "/%d.o", i));
		procs[i] = nob_cmd_run_async_and_reset(&cmd);
	}
	wait(procs);

	nob_log(NOB_INFO, "Linking blc-" BL_VERSION ".");
	{
		File_Paths files = {0};
		nob_read_entire_dir(BUILD_DIR, &files);

		cmd_append(&cmd, "c++", "-D_GNU_SOURCE", "-lrt", "-ldl", "-lm");
		if (IS_DEBUG) {
			abort();
			// cmd_append(cmd, "-DBL_DEBUG", "-DBL_ASSERT_ENABLE=1");
		} else {
			cmd_append(&cmd, BLC_FLAGS_RELEASE);
		}

		for (int i = 0; i < files.count; ++i) {
			if (ends_with(files.items[i], ".o")) cmd_append(&cmd, temp_sprintf(BUILD_DIR "/%s", files.items[i]));
		}

		cmd_append(&cmd, BUILD_DIR "/libyaml/libyaml.a", BUILD_DIR "/dyncall/dyncall.a");
		String_View libs = nob_sv_from_cstr(LLVM_LIBS);
		while (libs.count) {
			String_View lib = nob_sv_chop_by_delim(&libs, ' ');
			if (lib.count)
				cmd_append(&cmd, temp_sprintf("%s/" SV_Fmt, LLVM_LIB_DIR, SV_Arg(lib)));
		}

		// @Incomplete: Do we need these?
		nob_log(NOB_WARNING, "Using hardcoded /usr/lib/x86_64-linux-gnu/libz.so, /usr/lib/x86_64-linux-gnu/libzstd.so, /usr/lib/x86_64-linux-gnu/libtinfo.so while linking.");
		cmd_append(&cmd, "/usr/lib/x86_64-linux-gnu/libz.so", "/usr/lib/x86_64-linux-gnu/libzstd.so", "/usr/lib/x86_64-linux-gnu/libtinfo.so");
		cmd_append(&cmd, "-o", BIN_DIR "/blc");

		if (!cmd_run_sync_and_reset(&cmd)) exit(1);
	}

	run_shell_cmd("rm -f " BUILD_DIR "/*.o");

#endif
}

void finalize(void) {
#ifdef _WIN32
	if (!file_exists(BIN_DIR "/bl-lld.exe")) {
		copy_file("./deps/lld.exe", BIN_DIR "/bl-lld.exe");
	}
	if (!file_exists(BIN_DIR "/vswhere.exe")) {
		copy_file("./deps/vswhere-" VSWHERE_VERSION "/vswhere.exe", BIN_DIR "/vswhere.exe");
	}
#endif
}

void cleanup(void) {
#ifdef _WIN32
	run_shell_cmd("RD", "/S", "/Q", quote(BUILD_DIR));
#else
	run_shell_cmd("rm -fr " BUILD_DIR);
#endif
}
