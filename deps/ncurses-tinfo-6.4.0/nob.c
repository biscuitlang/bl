#if defined(__linux__)
#define TINFO_VERSION_MAJOR 6
#define TINFO_VERSION_MINOR 4
#define TINFO_VERSION_PATCH 0

#define TINFO_VERSION VERSION_STRING(TINFO_VERSION_MAJOR, TINFO_VERSION_MINOR, TINFO_VERSION_PATCH)

#define TINFO_BUILD_DIR BUILD_DIR "/tinfo"
#define TINFO_LIB "libtinfo.a"

const char *TINFO_LINK = TINFO_BUILD_DIR "/" TINFO_LIB;

void tinfo(void) {
	nob_log(NOB_INFO, "Providing tinfo-" TINFO_VERSION ".");
	mkdir_if_not_exists(BUILD_DIR "/tinfo");
	copy_file("./deps/ncurses-tinfo-" TINFO_VERSION "/libtinfo-linux-amd64.a", TINFO_LINK);
}
#endif
