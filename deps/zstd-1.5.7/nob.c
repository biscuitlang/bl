#if defined(__linux__)
#define ZSTD_VERSION_MAJOR 1
#define ZSTD_VERSION_MINOR 5
#define ZSTD_VERSION_PATCH 7

#define ZSTD_VERSION VERSION_STRING(ZSTD_VERSION_MAJOR, ZSTD_VERSION_MINOR, ZSTD_VERSION_PATCH)

#define ZSTD_BUILD_DIR BUILD_DIR "/zstd"
#define ZSTD_LIB       "libzstd.a"
#define ZSTD_LIB_DIR   "./deps/zstd-" ZSTD_VERSION "/lib"

const char *ZSTD_LINK = ZSTD_BUILD_DIR "/" ZSTD_LIB;

void zstd(void) {
	nob_log(NOB_INFO, "Compiling zstd-" ZSTD_VERSION ".");

	mkdir_if_not_exists(BUILD_DIR "/zstd");

	// Compress + decompress only: no dictBuilder, no legacy, no deprecated, no .S asm.
	const char *src[] = {
	    // common
	    ZSTD_LIB_DIR "/common/debug.c",
	    ZSTD_LIB_DIR "/common/entropy_common.c",
	    ZSTD_LIB_DIR "/common/error_private.c",
	    ZSTD_LIB_DIR "/common/fse_decompress.c",
	    ZSTD_LIB_DIR "/common/pool.c",
	    ZSTD_LIB_DIR "/common/threading.c",
	    ZSTD_LIB_DIR "/common/xxhash.c",
	    ZSTD_LIB_DIR "/common/zstd_common.c",
	    // compress
	    ZSTD_LIB_DIR "/compress/fse_compress.c",
	    ZSTD_LIB_DIR "/compress/hist.c",
	    ZSTD_LIB_DIR "/compress/huf_compress.c",
	    ZSTD_LIB_DIR "/compress/zstd_compress.c",
	    ZSTD_LIB_DIR "/compress/zstd_compress_literals.c",
	    ZSTD_LIB_DIR "/compress/zstd_compress_sequences.c",
	    ZSTD_LIB_DIR "/compress/zstd_compress_superblock.c",
	    ZSTD_LIB_DIR "/compress/zstd_double_fast.c",
	    ZSTD_LIB_DIR "/compress/zstd_fast.c",
	    ZSTD_LIB_DIR "/compress/zstd_lazy.c",
	    ZSTD_LIB_DIR "/compress/zstd_ldm.c",
	    ZSTD_LIB_DIR "/compress/zstd_opt.c",
	    ZSTD_LIB_DIR "/compress/zstd_preSplit.c",
	    ZSTD_LIB_DIR "/compress/zstdmt_compress.c",
	    // decompress
	    ZSTD_LIB_DIR "/decompress/huf_decompress.c",
	    ZSTD_LIB_DIR "/decompress/zstd_ddict.c",
	    ZSTD_LIB_DIR "/decompress/zstd_decompress.c",
	    ZSTD_LIB_DIR "/decompress/zstd_decompress_block.c",
	};
	const int src_num = ARRAY_LEN(src);

	Proc procs[ARRAY_LEN(src)];

	Cmd cmd = {0};
	for (int i = 0; i < src_num; ++i) {
		cmd_append(&cmd, "cc", "-c", src[i]);
		cmd_append(&cmd, "-D_GNU_SOURCE", "-O3", "-DNDEBUG");
		cmd_append(&cmd, "-DZSTD_LEGACY_SUPPORT=0", "-DZSTD_DISABLE_ASM", "-DXXH_NAMESPACE=ZSTD_");
		cmd_append(&cmd, "-I" ZSTD_LIB_DIR);
		cmd_append(&cmd, "-o", temp_sprintf(ZSTD_BUILD_DIR "/%d.o", i));
		procs[i] = nob_cmd_run_async_and_reset(&cmd);
	}
	wait(procs);
	ar(ZSTD_BUILD_DIR, ZSTD_LIB);
}
#endif