#define YAML_VERSION_MAJOR 0
#define YAML_VERSION_MINOR 2
#define YAML_VERSION_PATCH 5

#define YAML_VERSION VERSION_STRING(YAML_VERSION_MAJOR, YAML_VERSION_MINOR, YAML_VERSION_PATCH)

#ifdef _WIN32
#define YAML_LIB "libyaml.lib"
#else
#define YAML_LIB "libyaml.a"
#endif

void libyaml(void) {
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

#if _WIN32

	Proc procs[ARRAY_LEN(src)];

	Cmd cmd = {0};
	for (int i = 0; i < src_num; ++i) {
		cmd_append(&cmd, "cl", "-nologo", "-c", src[i]);
		cmd_append(&cmd, "-D_WIN32", "-D_WINDOWS", "-DNOMINMAX", "-D_HAS_EXCEPTIONS=0", "-GF", "-MD", "-O2", "-Oi", "-DNDEBUG", "-GL");
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
	wait(procs);
	lib(temp_sprintf("%s/libyaml", BUILD_DIR), YAML_LIB);
	run_shell_cmd("DEL", "/Q", "/F", quote(BUILD_DIR "\\libyaml\\*.obj"));

#elif __linux__

	Proc procs[ARRAY_LEN(src)];

	Cmd cmd = {0};
	for (int i = 0; i < src_num; ++i) {
		cmd_append(&cmd, "cc", "-c", src[i]);
		cmd_append(&cmd, "-D_GNU_SOURCE", "-O3", "-DNDEBUG");
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
	ar(temp_sprintf("%s/libyaml", BUILD_DIR), YAML_LIB);
	run_shell_cmd("rm -f " BUILD_DIR "/libyaml/*.o");

#endif
}
