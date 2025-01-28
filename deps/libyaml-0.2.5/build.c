void build_libyaml(void) {
	nob_log(NOB_INFO, "Building libyaml-" YAML_VERSION ".");

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

	const int src_num = ARRAYSIZE(src);
	Proc      procs[ARRAYSIZE(src)];

	Cmd cmd = {0};
	for (int i = 0; i < src_num; ++i) {
		cmd_append(&cmd, COMPILER, "-c", src[i]);
		cmd_extend(&cmd, &BASE_FLAGS);
		cmd_extend(&cmd, &BASE_FLAGS_RELEASE);
		cmd_append(&cmd,
		           "-DYAML_DECLARE_STATIC",
		           "-DYAML_VERSION_MAJOR=" STR(YAML_VERSION_MAJOR),
		           "-DYAML_VERSION_MINOR=" STR(YAML_VERSION_MINOR),
		           "-DYAML_VERSION_PATCH=" STR(YAML_VERSION_PATCH),
		           "-DYAML_VERSION_STRING=\\\"" YAML_VERSION "\\\"");
		cmd_append(&cmd, "-I./deps/libyaml-" YAML_VERSION "/include");
		cmd_append(&cmd, "-Fo\"" BUILD_DIR "/libyaml/\"");
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
			nob_read_entire_dir(BUILD_DIR "/libyaml", &files);

			cmd_append(&cmd, "lib", "-nologo", "-OUT:\"" BUILD_DIR "/libyaml/yaml.lib\"");
			for (int i = 0; i < files.count; ++i) {
				if (ends_with(files.items[i], ".obj")) cmd_append(&cmd, temp_sprintf(BUILD_DIR "/libyaml/%s", files.items[i]));
			}

			if (!cmd_run_sync_and_reset(&cmd)) exit(1);
		}

	run_shell_cmd("DEL", "/Q", "/F", quote(BUILD_DIR "\\libyaml\\*.obj"));
}