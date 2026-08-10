#if defined(__linux__)
#define ZSTD_VERSION_MAJOR 0
#define ZSTD_VERSION_MINOR 4
#define ZSTD_VERSION_PATCH 2

#define ZSTD_VERSION VERSION_STRING(ZSTD_VERSION_MAJOR, ZSTD_VERSION_MINOR, ZSTD_VERSION_PATCH)

#define ZSTD_BUILD_DIR BUILD_DIR "/zstd"
#define ZSTD_LIB "libzstd.a"

const char *ZSTD_LINK = ZSTD_BUILD_DIR "/" ZSTD_LIB;

void zstd(void) {
	nob_log(NOB_INFO, "Compiling zstd-" ZSTD_VERSION ".");

	mkdir_if_not_exists(BUILD_DIR "/zstd");

	const char *src[] = {
		"./deps/zstd-" ZSTD_VERSION "/lib/zstd_compress.c",
		"./deps/zstd-" ZSTD_VERSION "/lib/zstd_decompress.c",
		"./deps/zstd-" ZSTD_VERSION "/lib/fse.c",
		"./deps/zstd-" ZSTD_VERSION "/lib/huff0.c",
	};
	const int src_num = ARRAY_LEN(src);

	Proc procs[ARRAY_LEN(src)];

	Cmd cmd = {0};
	for (int i = 0; i < src_num; ++i) {
		cmd_append(&cmd, "cc", "-c", src[i]);
		cmd_append(&cmd, "-D_GNU_SOURCE", "-O3", "-DNDEBUG", "-DZSTD_LEGACY_SUPPORT=0");
		cmd_append(&cmd, "-I./deps/zstd-" ZSTD_VERSION "/lib");
		//cmd_append(&cmd, "-I./deps/zstd-" ZSTD_VERSION "/lib/legacy");
		cmd_append(&cmd, "-o", temp_sprintf(ZSTD_BUILD_DIR "/%d.o", i));
		procs[i] = nob_cmd_run_async_and_reset(&cmd);
	}
	wait(procs);
	ar(ZSTD_BUILD_DIR, ZSTD_LIB);
}
#endif
