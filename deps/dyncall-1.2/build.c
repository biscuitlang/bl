void build_dyncall(void) {
	nob_log(NOB_INFO, "Building dyncall-" DYNCALL_VERSION ".");

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
	cmd_append(&cmd, ASSEMBLER, "-nologo", "-Fo\"./" BUILD_DIR "/dyncall/\"", "-c", "./deps/dyncall-" DYNCALL_VERSION "/dyncall/dyncall_call_x64_generic_masm.asm");
	cmd_run_sync_and_reset(&cmd);
	cmd_append(&cmd, ASSEMBLER, "-nologo", "-Fo\"./" BUILD_DIR "/dyncall/\"", "-c", "./deps/dyncall-" DYNCALL_VERSION "/dyncallback/dyncall_callback_x64_masm.asm");
	cmd_run_sync_and_reset(&cmd);

	const int src_num = ARRAYSIZE(src);
	Proc      procs[ARRAYSIZE(src)];

	for (int i = 0; i < src_num; ++i) {
		cmd_append(&cmd, COMPILER, "-c", src[i]);
		cmd_extend(&cmd, &BASE_FLAGS);
		cmd_extend(&cmd, &BASE_FLAGS_RELEASE);
		cmd_append(&cmd, "-I./deps/dyncall-" DYNCALL_VERSION "/dyncall");
		cmd_append(&cmd, "-Fo\"" BUILD_DIR "/dyncall/\"");
		procs[i] = nob_cmd_run_async_and_reset(&cmd);
	}

	bool success = true;
	for (int i = 0; i < src_num; ++i) {
		success &= proc_wait(procs[i]);
	}
	if (!success) exit(1);

	// Link into single lib
	{
		File_Paths files = {0};
		nob_read_entire_dir(BUILD_DIR "/dyncall", &files);

		cmd_append(&cmd, "lib", "-nologo", "-OUT:\"" BUILD_DIR "/dyncall/dyncall.lib\"");
		for (int i = 0; i < files.count; ++i) {
			if (ends_with(files.items[i], ".obj")) cmd_append(&cmd, temp_sprintf(BUILD_DIR "/dyncall/%s", files.items[i]));
		}

		if (!cmd_run_sync_and_reset(&cmd)) exit(1);
	}

	run_shell_cmd("DEL", "/Q", "/F", quote(BUILD_DIR "\\dyncall\\*.obj"));
}