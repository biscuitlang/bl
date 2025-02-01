#define BL_VERSION_MAJOR 0
#define BL_VERSION_MINOR 13
#define BL_VERSION_PATCH 0

#define LLVM_REQUIRED 18

#define BL_VERSION       VERSION_STRING(BL_VERSION_MAJOR, BL_VERSION_MINOR, BL_VERSION_PATCH)
#define TRACY_VERSION    "0.9.1"
#define RPMALLOC_VERSION "1.4.4"
#define VSWHERE_VERSION  "2.8.4"

#include "find_llvm.c"

#ifdef __linux__
const char *LIBZ     = "";
const char *LIBZSTD  = "";
const char *LIBTINFO = "";
#elif __APPLE__
const char *MACOS_SDK = "";
const char *LIBZ      = "";
const char *LIBZSTD   = "";
const char *LIBCURSES = "";
#endif

void find_deps(void);

// Compilation database recording...
#if BL_EXPORT_COMPILE_COMMANDS
String_Builder db;
char          *db_dir;
void           db_begin(void);
void           db_end(void);
void           db_add_entry(const char *file, Cmd cmd);
#else
#define db_begin()
#define db_end()
#define db_add_entry(a, b)
#endif

void setup(void) {
	find_llvm();
	find_deps();
}

void blc(void) {
	const char *config_name = IS_DEBUG ? "DEBUG" : (ASSERT_ENABLE ? "ASSERT" : "RELEASE");
	nob_log(NOB_INFO, temp_sprintf("Compiling blc-" BL_VERSION " (%s).", config_name));

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

	db_begin();

#ifdef _WIN32

	Cmd  cmd = {0};
	Proc procs[ARRAY_LEN(src)];

	for (int i = 0; i < src_num; ++i) {
		const bool is_cxx = ends_with(src[i], ".cpp");
		cmd_append(&cmd,
		           "cl",
		           "-nologo",
		           "-W4",
		           "-wd4100",
		           "-wd4127",
		           "-wd4189",
		           "-wd4200",
		           "-wd4244",
		           "-wd4245",
		           "-wd4310",
		           "-wd4324",
		           "-wd4389",
		           "-wd4456",
		           "-wd4457",
		           "-wd4702",
		           "-wd4996",
		           "-c",
		           src[i]);
		cmd_extend(&cmd, &include_paths);
		cmd_append(&cmd,
		           "-DBL_VERSION_MAJOR=" STR(BL_VERSION_MAJOR),
		           "-DBL_VERSION_MINOR=" STR(BL_VERSION_MINOR),
		           "-DBL_VERSION_PATCH=" STR(BL_VERSION_PATCH),
		           "-DYAML_DECLARE_STATIC");
		cmd_append(&cmd, "-D_WIN32", "-D_WINDOWS", "-DNOMINMAX", "-D_HAS_EXCEPTIONS=0", "-GF", "-MD");
		if (IS_DEBUG) {
			cmd_append(&cmd, "-Od", "-Zi", "-DBL_DEBUG", "-FS");
		} else {
			cmd_append(&cmd, "-O2", "-Oi", "-DNDEBUG", "-GL");
		}
		cmd_append(&cmd, temp_sprintf("-DBL_ASSERT_ENABLE=%d", ASSERT_ENABLE ? 1 : 0));
		if (BL_SIMD_ENABLE) cmd_append(&cmd, "-DBL_USE_SIMD", "-arch:AVX");
		if (BL_RPMALLOC_ENABLE) cmd_append(&cmd, "-DBL_RPMALLOC_ENABLE=1");
		cmd_append(&cmd, is_cxx ? "-std:c++17" : "-std:c11");
		db_add_entry(src[i], cmd);

		cmd_append(&cmd, "-Fo\"" BUILD_DIR "/\"");
		procs[i] = nob_cmd_run_async_and_reset(&cmd);
	}
	wait(procs);

	nob_log(NOB_INFO, "Linking blc-" BL_VERSION ".");
	{
		File_Paths files = {0};
		nob_read_entire_dir(BUILD_DIR, &files);

		cmd_append(&cmd, "cl", "-nologo");
		cmd_append(&cmd, "-D_WIN32", "-D_WINDOWS", "-DNOMINMAX", "-D_HAS_EXCEPTIONS=0", "-GF", "-MD", "-FS");
		if (IS_DEBUG) {
			cmd_append(&cmd, "-Od", "-Zi", "-DBL_DEBUG");
		} else {
			cmd_append(&cmd, "-O2", "-Oi", "-DNDEBUG", "-GL");
		}
		cmd_append(&cmd, temp_sprintf("-DBL_ASSERT_ENABLE=%d", ASSERT_ENABLE ? 1 : 0));
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

	shell("del", "/Q", "/F", "\"" BUILD_DIR "\\*.obj\"");

#elif defined(__linux__) || defined(__APPLE__)

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
#ifdef __APPLE__
		cmd_append(&cmd, "-arch", "arm64", "-isysroot", MACOS_SDK);
		if (IS_DEBUG) {
			cmd_append(&cmd, "-O0", "-g", "-DBL_DEBUG");
		} else {
			cmd_append(&cmd, "-O3", "-DNDEBUG");
		}
#else
		cmd_append(&cmd, "-D_GNU_SOURCE");
		if (IS_DEBUG) {
			cmd_append(&cmd, "-O0", "-ggdb", "-DBL_DEBUG");
		} else {
			cmd_append(&cmd, "-O3", "-DNDEBUG");
		}
#endif
		cmd_append(&cmd, temp_sprintf("-DBL_ASSERT_ENABLE=%d", ASSERT_ENABLE ? 1 : 0));
		if (BL_SIMD_ENABLE) nob_log(NOB_WARNING, "BL_SIMD_ENABLE not supported on this platform.");
		if (BL_RPMALLOC_ENABLE) cmd_append(&cmd, "-DBL_RPMALLOC_ENABLE=1");
		cmd_append(&cmd, is_cxx ? "-std=c++17" : "-std=gnu11");
		db_add_entry(src[i], cmd);

		cmd_append(&cmd, "-o", temp_sprintf(BUILD_DIR "/%d.o", i));
		procs[i] = nob_cmd_run_async_and_reset(&cmd);
	}
	wait(procs);

	nob_log(NOB_INFO, "Linking blc-" BL_VERSION ".");
	{
		File_Paths files = {0};
		nob_read_entire_dir(BUILD_DIR, &files);
#ifdef __APPLE__
		cmd_append(&cmd, "c++", "-arch", "arm64", "-lm", "-mmacosx-version-min=14.3");
#else
		cmd_append(&cmd, "c++", "-D_GNU_SOURCE", "-lrt", "-ldl", "-lm", "-rdynamic", "-Wl,--export-dynamic");
#endif

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
#ifdef __APPLE__
		cmd_append(&cmd, LIBZ, LIBZSTD, LIBCURSES);
#else
		cmd_append(&cmd, LIBZ, LIBZSTD, LIBTINFO);
#endif
		cmd_append(&cmd, "-o", BIN_DIR "/blc");

		if (!cmd_run_sync_and_reset(&cmd)) exit(1);
	}

	shell("rm -f " BUILD_DIR "/*.o");

#endif

	db_end();
}

void blc_runtime(void) {
	nob_log(NOB_INFO, "Compiling BL runtime.");
	Cmd cmd = {0};

#ifdef _WIN32
	cmd_append(&cmd, "cl", "-c", "-nologo", "./src/dlib/dlib_runtime.c", "-O2", "-Oi", "-DNDEBUG", "-I./src", "-Fo\"./lib/bl/api/std/dlib/win32/\"");
	nob_cmd_run_sync_and_reset(&cmd);
	cmd_append(&cmd, "lib", "-nologo", "./lib/bl/api/std/dlib/win32/dlib_runtime.obj", "-OUT:\"./lib/bl/api/std/dlib/win32/dlib.lib\"");
#elif __linux__
	cmd_append(&cmd, "cc", "-c", "./src/dlib/dlib_runtime.c", "-I./src", "-D_GNU_SOURCE", "-O3", "-DNDEBUG", "-o", "./lib/bl/api/std/dlib/linux/libdlib.a");
#elif __APPLE__
	cmd_append(&cmd, "cc", "-c", "./src/dlib/dlib_runtime.c", "-I./src", "-arch", "arm64", "-O3", "-DNDEBUG", "-o", "./lib/bl/api/std/dlib/darwin/libdlib.a");
#endif

	nob_cmd_run_sync_and_reset(&cmd);
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
	if (!shell("RD", "/S", "/Q", "\"" BUILD_DIR "\"")) exit(1);
#else
	if (!shell("rm -fr " BUILD_DIR)) exit(1);
#endif
}

#ifdef __linux__
void find_deps(void) {
	LIBZ = shell("find /usr/lib /usr/local/lib -name \"libz.a\" -print -quit 2>/dev/null");
	if (!strok(LIBZ)) {
		nob_log(NOB_ERROR, "Unable to find 'libz'.");
		exit(1);
	}
	nob_log(NOB_INFO, "Using 'libz' '%s'.", LIBZ);

	LIBZSTD = shell("find /usr/lib /usr/local/lib -name \"libzstd.a\" -print -quit 2>/dev/null");
	if (!strok(LIBZSTD)) {
		nob_log(NOB_ERROR, "Unable to find 'libzstd'.");
		exit(1);
	}
	nob_log(NOB_INFO, "Using 'libzstd' '%s'.", LIBZSTD);

	LIBTINFO = shell("find /usr/lib /usr/local/lib -name \"libtinfo.a\" -print -quit 2>/dev/null");
	if (!strok(LIBTINFO)) {
		nob_log(NOB_ERROR, "Unable to find 'libtinfo'.");
		exit(1);
	}
	nob_log(NOB_INFO, "Using 'libtinfo' '%s'.", LIBTINFO);
}

#elif __APPLE__
void find_deps(void) {
	if (!strok(shell("which brew"))) {
		nob_log(NOB_ERROR, "Homebrew package manager not found. We currently require dependencies installed using homebrew because in some cases the MacOS Command Line tools does not provide static libraries.");
		exit(1);
	}

	MACOS_SDK = shell("xcrun --show-sdk-path 2>/dev/null");
	if (!strok(MACOS_SDK)) {
		nob_log(NOB_ERROR, "Unable to find MacOS SDK, you might try to run 'xcode-select --install'.");
		exit(1);
	}
	nob_log(NOB_INFO, "Using MacOS SDK '%s'.", MACOS_SDK);

	const char *libz_prefix = shell("brew --prefix zlib 2>/dev/null");
	if (!strok(libz_prefix)) {
		nob_log(NOB_ERROR, "Unable to find zlib brew package. Run 'brew install zlib'.");
		exit(1);
	}
	LIBZ = shell(temp_sprintf(temp_sprintf("find %s/lib -name \"libz.a\" -print -quit 2>/dev/null", libz_prefix)));
	if (!strok(LIBZ)) {
		nob_log(NOB_ERROR, "Unable to find 'libz'.");
		exit(1);
	}
	nob_log(NOB_INFO, "Using 'libz' '%s'.", LIBZ);

	const char *zstd_prefix = shell("brew --prefix zstd 2>/dev/null");
	if (!strok(zstd_prefix)) {
		nob_log(NOB_ERROR, "Unable to find zstd brew package.");
		exit(1);
	}
	LIBZSTD = shell(temp_sprintf(temp_sprintf("find %s/lib -name \"libzstd.a\" -print -quit 2>/dev/null", zstd_prefix)));
	if (!strok(LIBZSTD)) {
		nob_log(NOB_ERROR, "Unable to find 'libzstd'.");
		exit(1);
	}
	nob_log(NOB_INFO, "Using 'libzstd' '%s'.", LIBZSTD);

	// This is required by LLVM for 3 or 4 functions, we should find the way how to not require this whole shit...
	const char *ncurses_prefix = shell("brew --prefix ncurses 2>/dev/null");
	if (!strok(ncurses_prefix)) {
		nob_log(NOB_ERROR, "Unable to find ncurses brew package. Run 'brew install zlib'.");
		exit(1);
	}
	LIBCURSES = shell(temp_sprintf(temp_sprintf("find %s/lib -name \"libcurses.a\" -print -quit 2>/dev/null", ncurses_prefix)));
	if (!strok(LIBCURSES)) {
		nob_log(NOB_ERROR, "Unable to find 'libcurses'.");
		exit(1);
	}
	nob_log(NOB_INFO, "Using 'libcurses' '%s'.", LIBCURSES);
}
#else
void find_deps(void) {
}
#endif

#if BL_EXPORT_COMPILE_COMMANDS
void db_begin(void) {
	db_dir = (char *)get_current_dir_temp();
	for (int i = 0; db_dir[i] != '\0'; ++i) {
		if (db_dir[i] == '\\') db_dir[i] = '/';
	}

	sb_append_cstr(&db, "[");
}

void db_end(void) {
	sb_append_cstr(&db, "\n]");
	write_entire_file("compile_commands.json", db.items, db.count);
}

void db_add_entry(const char *file, Cmd cmd) {
	// Your json with love...
	sb_append_cstr(&db, db.items[db.count - 1] == '[' ? "\n{\n" : ",\n{\n");
	sb_append_cstr(&db, temp_sprintf("\"directory\":\"%s\",\n", db_dir));
	sb_append_cstr(&db, "\"command\":\"");
	for (int i = 0; i < cmd.count; ++i) {
		sb_append_cstr(&db, cmd.items[i]);
		sb_append_cstr(&db, " ");
	}
	sb_append_cstr(&db, "\",\n");
	sb_append_cstr(&db, "\"file\":\"");
	sb_append_cstr(&db, file);
	sb_append_cstr(&db, "\"\n");
	sb_append_cstr(&db, "}");
}
#endif