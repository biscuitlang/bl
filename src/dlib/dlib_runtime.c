#include "basic_types.h"
#include "config.h"

#if BL_PLATFORM_WIN

#include <windows.h>

BL_EXPORT void *__dlib_open(const char *libname) {
	if (!libname || strlen(libname) == 0) {
		return GetModuleHandleA(NULL);
	}
	return LoadLibraryA(libname);
}

BL_EXPORT void __dlib_close(void *lib) {
	if (!lib) return;
	FreeLibrary(lib);
}

BL_EXPORT void *__dlib_symbol(void *lib, const char *symname) {
	return GetProcAddress(lib, symname);
}

#else

#include <string.h>
#include <dlfcn.h>

BL_EXPORT void *__dlib_open(const char *libname) {
	if (!libname || strlen(libname) == 0) {
		return dlopen(NULL, RTLD_LAZY);
	}
	return dlopen(libname, RTLD_LAZY);
}

BL_EXPORT void __dlib_close(void *lib) {
	if (!lib) return;
	dlclose(lib);
}

BL_EXPORT void *__dlib_symbol(void *lib, const char *symname) {
	return dlsym(lib, symname);
}

#endif