#if defined(__linux__)
#define ZLIB_VERSION_MAJOR 1
#define ZLIB_VERSION_MINOR 3
#define ZLIB_VERSION_PATCH 2

#define ZLIB_VERSION VERSION_STRING(ZLIB_VERSION_MAJOR, ZLIB_VERSION_MINOR, ZLIB_VERSION_PATCH)

#define ZLIB_BUILD_DIR BUILD_DIR "/zlib"
#define ZLIB_LIB "libz.a"

const char *ZLIB_LINK = ZLIB_BUILD_DIR "/" ZLIB_LIB;

void zlib(void) {
	nob_log(NOB_INFO, "Compiling zlib-" ZLIB_VERSION ".");

	mkdir_if_not_exists(BUILD_DIR "/zlib");

	const char *src[] = {
	    "./deps/zlib-" ZLIB_VERSION "/adler32.c",
	    "./deps/zlib-" ZLIB_VERSION "/compress.c",
	    "./deps/zlib-" ZLIB_VERSION "/crc32.c",
	    "./deps/zlib-" ZLIB_VERSION "/deflate.c",
	    "./deps/zlib-" ZLIB_VERSION "/gzclose.c",
	    "./deps/zlib-" ZLIB_VERSION "/gzlib.c",
	    "./deps/zlib-" ZLIB_VERSION "/gzread.c",
	    "./deps/zlib-" ZLIB_VERSION "/gzwrite.c",
	    "./deps/zlib-" ZLIB_VERSION "/inflate.c",
	    "./deps/zlib-" ZLIB_VERSION "/infback.c",
	    "./deps/zlib-" ZLIB_VERSION "/inftrees.c",
	    "./deps/zlib-" ZLIB_VERSION "/inffast.c",
	    "./deps/zlib-" ZLIB_VERSION "/trees.c",
	    "./deps/zlib-" ZLIB_VERSION "/uncompr.c",
	    "./deps/zlib-" ZLIB_VERSION "/zutil.c",
	};
	const int src_num = ARRAY_LEN(src);

	Proc procs[ARRAY_LEN(src)];

	Cmd cmd = {0};
	for (int i = 0; i < src_num; ++i) {
		cmd_append(&cmd, "cc", "-c", src[i]);
		cmd_append(&cmd, "-D_GNU_SOURCE", "-O3", "-DNDEBUG");
		cmd_append(&cmd, "-I./deps/zlib-" ZLIB_VERSION);
		cmd_append(&cmd, "-o", temp_sprintf(ZLIB_BUILD_DIR "/%d.o", i));
		procs[i] = nob_cmd_run_async_and_reset(&cmd);
	}
	wait(procs);
	ar(ZLIB_BUILD_DIR, ZLIB_LIB);
}
#endif
