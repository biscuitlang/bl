#define SHELL         "bash", "-c"
#define FLAGS_DEBUG   "-D_GNU_SOURCE", "-rdynamic"
#define FLAGS_RELEASE "-D_GNU_SOURCE", "-O3", "-DNDEBUG", "-rdynamic"

#include "find_llvm.c"

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

void build_dyncall(void) {
	if (file_exists(BUILD_DIR "/dyncall/dyncall.a")) return;

	nob_log(NOB_INFO, "Compiling dyncall-" DYNCALL_VERSION ".");
	mkdir_if_not_exists(BUILD_DIR "/dyncall");

	const char *src[] = {
	    "./deps/dyncall-" DYNCALL_VERSION "/dyncall/dyncall_vector.c",
	    "./deps/dyncall-" DYNCALL_VERSION "/dyncall/dyncall_struct.c",
	    "./deps/dyncall-" DYNCALL_VERSION "/dyncall/dyncall_api.c",
	    "./deps/dyncall-" DYNCALL_VERSION "/dyncall/dyncall_callvm.c",
	    "./deps/dyncall-" DYNCALL_VERSION "/dyncall/dyncall_callvm_base.c",
	    "./deps/dyncall-" DYNCALL_VERSION "/dyncall/dyncall_callf.c",
	    "./deps/dyncall-" DYNCALL_VERSION "/dyncallback/dyncall_thunk.c",
	    "./deps/dyncall-" DYNCALL_VERSION "/dyncallback/dyncall_alloc_wx.c",
	    "./deps/dyncall-" DYNCALL_VERSION "/dyncallback/dyncall_args.c",
	    "./deps/dyncall-" DYNCALL_VERSION "/dyncallback/dyncall_callback.c",
	    "./deps/dyncall-" DYNCALL_VERSION "/dynload/dynload.c",
	    "./deps/dyncall-" DYNCALL_VERSION "/dynload/dynload_syms.c",
	};

	Cmd cmd = {0};
	cmd_append(&cmd, "cc", "-o", BUILD_DIR "/dyncall/dyncall_call.S.o", "-c", "./deps/dyncall-" DYNCALL_VERSION "/dyncall/dyncall_call.S");
	cmd_run_sync_and_reset(&cmd);
	cmd_append(&cmd, "cc", "-o", BUILD_DIR "/dyncall/dyncall_callback_arch.S.o", "-c", "./deps/dyncall-" DYNCALL_VERSION "/dyncallback/dyncall_callback_arch.S");
	cmd_run_sync_and_reset(&cmd);

	Proc procs[ARRAY_LEN(src)];
	for (int i = 0; i < ARRAY_LEN(src); ++i) {
		cmd_append(&cmd, "cc", "-c", src[i]);
		cmd_append(&cmd, FLAGS_RELEASE);
		cmd_append(&cmd, "-I./deps/dyncall-" DYNCALL_VERSION "/dyncall");
		cmd_append(&cmd, "-o", temp_sprintf(BUILD_DIR "/dyncall/%d.o", i));
		procs[i] = nob_cmd_run_async_and_reset(&cmd);
	}
	wait(procs);
	ar(temp_sprintf("%s/dyncall", BUILD_DIR), "dyncall.a");
	run_shell_cmd("rm -f " BUILD_DIR "/dyncall/*.o");
}

void build_libyaml(void) {
	if (file_exists(BUILD_DIR "/libyaml/libyaml.a")) return;
	nob_log(NOB_INFO, "Compiling libyaml-" YAML_VERSION ".");

	mkdir_if_not_exists(BUILD_DIR "/libyaml");

	const char *src[] = {
	    "./deps/libyaml-" YAML_VERSION "/src/api.c",
	    "./deps/libyaml-" YAML_VERSION "/src/dumper.c",
	    "./deps/libyaml-" YAML_VERSION "/src/emitter.c",
	    "./deps/libyaml-" YAML_VERSION "/src/loader.c",
	    "./deps/libyaml-" YAML_VERSION "/src/parser.c",
	    "./deps/libyaml-" YAML_VERSION "/src/reader.c",
	    "./deps/libyaml-" YAML_VERSION "/src/scanner.c",
	    "./deps/libyaml-" YAML_VERSION "/src/writer.c",
	};

	const int src_num = ARRAY_LEN(src);
	Proc      procs[ARRAY_LEN(src)];

	Cmd cmd = {0};
	for (int i = 0; i < src_num; ++i) {
		cmd_append(&cmd, "cc", "-c", src[i]);
		cmd_append(&cmd, FLAGS_RELEASE);
		cmd_append(&cmd,
		           "-DYAML_DECLARE_STATIC",
		           "-DYAML_VERSION_MAJOR=" STR(YAML_VERSION_MAJOR),
		           "-DYAML_VERSION_MINOR=" STR(YAML_VERSION_MINOR),
		           "-DYAML_VERSION_PATCH=" STR(YAML_VERSION_PATCH),
		           "-DYAML_VERSION_STRING=\"" YAML_VERSION "\"");
		cmd_append(&cmd, "-I./deps/libyaml-" YAML_VERSION "/include");
		cmd_append(&cmd, "-o", temp_sprintf(BUILD_DIR "/libyaml/%d.o", i));
		procs[i] = nob_cmd_run_async_and_reset(&cmd);
	}
	wait(procs);
	ar(temp_sprintf("%s/libyaml", BUILD_DIR), "libyaml.a");
	run_shell_cmd("rm -f " BUILD_DIR "/libyaml/*.o");
}

void build_blc(void) {
	nob_log(NOB_INFO, "Compiling blc-" BL_VERSION ".");

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

	Cmd  cmd = {0};
	Proc procs[ARRAY_LEN(src)];

	for (int i = 0; i < src_num; ++i) {
		const bool is_cxx = ends_with(src[i], ".cpp");
		cmd_append(&cmd, is_cxx ? "c++" : "cc", "-c", src[i]);
		cmd_append(&cmd, "-I./deps/dyncall-" DYNCALL_VERSION "/dyncall");
		cmd_append(&cmd, "-I./deps/dyncall-" DYNCALL_VERSION "/dynload");
		cmd_append(&cmd, "-I./deps/dyncall-" DYNCALL_VERSION "/dyncallback");
		cmd_append(&cmd, "-I./deps/libyaml-" YAML_VERSION "/include");
		cmd_append(&cmd, "-I./deps/tracy-" TRACY_VERSION "/public/tracy");
		cmd_append(&cmd, "-I./deps/rpmalloc-" RPMALLOC_VERSION "/rpmalloc");
		cmd_append(&cmd, temp_sprintf("-I%s", LLVM_INCLUDE_DIR));
		cmd_append(&cmd,
		           "-DBL_VERSION_MAJOR=" STR(BL_VERSION_MAJOR),
		           "-DBL_VERSION_MINOR=" STR(BL_VERSION_MINOR),
		           "-DBL_VERSION_PATCH=" STR(BL_VERSION_PATCH),
		           "-DYAML_DECLARE_STATIC");
		if (IS_DEBUG) {
			abort();
			// cmd_append(cmd, "-D_GNU_SOURCE", BASE_FLAGS_DEBUG, "-DBL_DEBUG", "-DBL_ASSERT_ENABLE=1");
		} else {
			cmd_append(&cmd, FLAGS_RELEASE);
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
			cmd_append(&cmd, FLAGS_RELEASE);
		}

		for (int i = 0; i < files.count; ++i) {
			if (ends_with(files.items[i], ".o")) cmd_append(&cmd, temp_sprintf(BUILD_DIR "/%s", files.items[i]));
		}

		cmd_append(&cmd, BUILD_DIR "/libyaml/libyaml.a", BUILD_DIR "/dyncall/dyncall.a");
		String_View libs = nob_sv_from_cstr(LLVM_LIBS);
		while (libs.count) {
			String_View lib = nob_sv_chop_by_delim(&libs, ' ');
			if (lib.count)
				cmd_append(&cmd, temp_sprintf("%s/" SV_Fmt, LLVM_LIB_DIR, lib));
		}

		// @Incomplete: Do we need these?
		nob_log(NOB_WARNING, "Using hardcoded /usr/lib/x86_64-linux-gnu/libz.so, /usr/lib/x86_64-linux-gnu/libzstd.so, /usr/lib/x86_64-linux-gnu/libtinfo.so while linking.");
		cmd_append(&cmd, "/usr/lib/x86_64-linux-gnu/libz.so", "/usr/lib/x86_64-linux-gnu/libzstd.so", "/usr/lib/x86_64-linux-gnu/libtinfo.so");
		cmd_append(&cmd, "-o", BIN_DIR "/blc");

		if (!cmd_run_sync_and_reset(&cmd)) exit(1);
	}

	run_shell_cmd("rm -f " BUILD_DIR "/*.o");

	/*
	if (BL_EXPORT_COMPILE_COMMANDS) {
	    nob_log(NOB_INFO, "Generate compilation database.");
	    String_Builder sb = {0};
	    sb_append_cstr(&sb, "[\n");
	    for (int i = 0; i < src_num; ++i) {
	        const bool is_cxx = ends_with(src[i], ".cpp");
	        cmd_append(&cmd, is_cxx ? CXX_COMPILER : C_COMPILER);
	        cmd_append(&cmd, src[i]);
	        cmd_append_bl_flags(&cmd);
	        cmd_append(&cmd, is_cxx ? CXX_VER : C_VER);
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
	*/
}

void finalize(void) {
}

void cleanup(void) {
	run_shell_cmd("rm -fr " BUILD_DIR);
}