#include "basic_types.h"
#include "config.h"

#if BL_PLATFORM_WIN

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

extern s32 __os_start();

s32 main(s32 argc, char **argv) {
	return __os_start();
}

s32 WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, s32 nShowCmd) {
	return __os_start();
}

#else

extern s32 __os_start(s32 argc, char **argv);

s32 main(s32 argc, char **argv) {
	return __os_start(argc, argv);
}

#endif