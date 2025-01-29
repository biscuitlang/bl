#define DYNCALL_VERSION "1.2"

#ifdef _WIN32
#define DYNCALL_LIB "dyncall.lib"
#else
#define DYNCALL_LIB "dyncall.a"
#endif

void dyncall(void) {
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

#ifdef _WIN32

	Cmd cmd = {0};
	cmd_append(&cmd, "ml64", "-nologo", "-Fo\"./" BUILD_DIR "/dyncall/\"", "-c", "./deps/dyncall-" DYNCALL_VERSION "/dyncall/dyncall_call_x64_generic_masm.asm");
	cmd_run_sync_and_reset(&cmd);
	cmd_append(&cmd, "ml64", "-nologo", "-Fo\"./" BUILD_DIR "/dyncall/\"", "-c", "./deps/dyncall-" DYNCALL_VERSION "/dyncallback/dyncall_callback_x64_masm.asm");
	cmd_run_sync_and_reset(&cmd);

	Proc procs[ARRAY_LEN(src)];
	for (int i = 0; i < ARRAY_LEN(src); ++i) {
		cmd_append(&cmd, "cl", "-nologo", "-c", src[i]);
		cmd_append(&cmd, "-D_WIN32", "-D_WINDOWS", "-DNOMINMAX", "-D_HAS_EXCEPTIONS=0", "-GF", "-MD", "-O2", "-Oi", "-DNDEBUG", "-GL");
		cmd_append(&cmd, "-I./deps/dyncall-" DYNCALL_VERSION "/dyncall");
		cmd_append(&cmd, "-Fo\"" BUILD_DIR "/dyncall/\"", );
		procs[i] = nob_cmd_run_async_and_reset(&cmd);
	}
	wait(procs);
	lib(temp_sprintf("%s/dyncall", BUILD_DIR), DYNCALL_LIB);

#elif defined(__linux__) || defined(__APPLE__)

	Cmd cmd = {0};
	cmd_append(&cmd, "cc", "-o", BUILD_DIR "/dyncall/dyncall_call.S.o", "-c", "./deps/dyncall-" DYNCALL_VERSION "/dyncall/dyncall_call.S");
	cmd_run_sync_and_reset(&cmd);
	cmd_append(&cmd, "cc", "-o", BUILD_DIR "/dyncall/dyncall_callback_arch.S.o", "-c", "./deps/dyncall-" DYNCALL_VERSION "/dyncallback/dyncall_callback_arch.S");
	cmd_run_sync_and_reset(&cmd);

	Proc procs[ARRAY_LEN(src)];
	for (int i = 0; i < ARRAY_LEN(src); ++i) {
		cmd_append(&cmd, "cc", "-c", src[i]);
#ifdef __APPLE__
		cmd_append(&cmd, "-arch", "arm64", "-O3", "-DNDEBUG");
#else
		cmd_append(&cmd, "-D_GNU_SOURCE", "-O3", "-DNDEBUG");
#endif
		cmd_append(&cmd, "-I./deps/dyncall-" DYNCALL_VERSION "/dyncall");
		cmd_append(&cmd, "-o", temp_sprintf(BUILD_DIR "/dyncall/%d.o", i));
		procs[i] = nob_cmd_run_async_and_reset(&cmd);
	}
	wait(procs);
	ar(temp_sprintf("%s/dyncall", BUILD_DIR), DYNCALL_LIB);

#endif
}
