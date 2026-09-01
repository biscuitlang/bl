#ifndef BL_CONFIG_H
#define BL_CONFIG_H

#define STR_HELPER(x) #x
#define STR(x)        STR_HELPER(x)

#define bl_version_hash(major, minor, patch) (((major) & 0xFF) << 16 | ((minor) & 0xFF) << 8 | ((patch) & 0xFF) << 0)

#define BL_VERSION STR(BL_VERSION_MAJOR) "." STR(BL_VERSION_MINOR) "." STR(BL_VERSION_PATCH)

#define BL_CONFIG_FILE "etc/bl.yaml"
#define BL_API_DIR     "../lib/bl/api"

// MULTIPLATFORM: os preload depends on target build platform and should be chosen in runtime later!
#ifdef _WIN32
#define BL_PLATFORM_WIN   1
#define BL_PLATFORM_MACOS 0
#define BL_PLATFORM_LINUX 0
#define BL_EXPORT         __declspec(dllexport)
#define BL_VSWHERE_EXE    "vwshere.exe"
#define BL_LINKER         "bl-lld.exe"
#elif __APPLE__
#define BL_PLATFORM_WIN   0
#define BL_PLATFORM_MACOS 1
#define BL_PLATFORM_LINUX 0
#define BL_LINKER         "ld"
#define BL_EXPORT
#elif __linux__
#define BL_PLATFORM_WIN   0
#define BL_PLATFORM_MACOS 0
#define BL_PLATFORM_LINUX 1
#define BL_EXPORT         __attribute__((used))
#define BL_LINKER         "ld"
#else
#error "Unknown platform"
#endif


#ifdef __clang__
#if defined(__apple_build_version__)
#define BL_COMPILER_APPLE_CLANG 1
#endif
#define BL_COMPILER_CLANG 1
#define BL_COMPILER_GNUC  0
#define BL_COMPILER_MSVC  0
#elif defined(__GNUC__) || defined(__MINGW32__)
#define BL_COMPILER_APPLE_CLANG 0
#define BL_COMPILER_CLANG 0
#define BL_COMPILER_GNUC  1
#define BL_COMPILER_MSVC  0
#elif _MSC_VER
#define BL_COMPILER_APPLE_CLANG 0
#define BL_COMPILER_CLANG 0
#define BL_COMPILER_GNUC  0
#define BL_COMPILER_MSVC  1
#endif

#define ENV_PATH "PATH"

#if BL_PLATFORM_WIN
#define ENVPATH_SEPARATORC ';'
#define ENVPATH_SEPARATOR  ";"
#define MSVC_CRT           "MSVCRT.dll"
#define KERNEL32           "Kernel32.dll"
#define SHLWAPI            "Shlwapi.dll"
#else
#define ENVPATH_SEPARATORC ':'
#define ENVPATH_SEPARATOR  ":"
#endif

#define CONFIG_SEPARATOR ","

#define BUILD_API_FILE    "build/build.bl"
#define BUILTIN_FILE      "builtin/a.bl"
#define ASAN_MODULE_PATH  "builtin/asan"
#define ENTRY_MODULE_PATH "builtin/entry"

// NOTE: We are converting all backslashes on Windows to the forward slashes.
#define PATH_SEPARATOR  "/"
#define PATH_SEPARATORC '/'

#ifdef BL_DEBUG
#define ASSERT_ON_CMP_ERROR 0
#define BL_MAGIC_ENABLE     1
#else
#define ASSERT_ON_CMP_ERROR 0
#define BL_MAGIC_ENABLE     0
#endif

#define VM_STACK_SIZE               2097152 // 2MB
#define VM_COMPTIME_CALL_STACK_SIZE 131072  // 128kB

#define MODULE_CONFIG_FILE "module.yaml"
#define BUILD_SCRIPT_FILE  "build.bl"

#endif // BL_CONFIG_H
