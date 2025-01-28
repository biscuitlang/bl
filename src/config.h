//************************************************************************************************
// bl
//
// File:   config.c
// Author: Martin Dorazil
// Date:   3/12/18
//
// Copyright 2018 Martin Dorazil
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//************************************************************************************************

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define BL_VERSION STR(BL_VERSION_MAJOR) "." STR(BL_VERSION_MINOR) "." STR(BL_VERSION_PATCH)

#define BL_CONFIG_FILE "etc/bl.yaml"
#define BL_API_DIR "../lib/bl/api"

// MULTIPLATFORM: os preload depends on target build platform and should be chosen in runtime later!
#ifdef _WIN32
#define BL_PLATFORM_WIN 1
#define BL_PLATFORM_MACOS 0
#define BL_PLATFORM_LINUX 0
#define BL_EXPORT __declspec(dllexport)
#define BL_VSWHERE_EXE "vswhere.exe"
#define BL_LINKER "bl-lld.exe"
#elif __APPLE__
#define BL_PLATFORM_WIN 0
#define BL_PLATFORM_MACOS 1
#define BL_PLATFORM_LINUX 0
#define BL_EXPORT
#define BL_LINKER ""
#elif __linux__
#define BL_PLATFORM_WIN 0
#define BL_PLATFORM_MACOS 0
#define BL_PLATFORM_LINUX 1
#define BL_EXPORT __attribute__((used))
#define BL_LINKER ""
#else
#error "Unknown platform"
#endif

#ifdef __clang__
#define BL_COMPILER_CLANG 1
#define BL_COMPILER_GNUC 0
#define BL_COMPILER_MSVC 0
#elif defined(__GNUC__) || defined(__MINGW32__)
#define BL_COMPILER_CLANG 0
#define BL_COMPILER_GNUC 1
#define BL_COMPILER_MSVC 0
#elif _MSC_VER
#define BL_COMPILER_CLANG 0
#define BL_COMPILER_GNUC 0
#define BL_COMPILER_MSVC 1
#endif

#define ENV_PATH "PATH"

#if BL_PLATFORM_WIN
#define ENVPATH_SEPARATORC ';'
#define ENVPATH_SEPARATOR ";"
#define MSVC_CRT "MSVCRT.dll"
#define KERNEL32 "Kernel32.dll"
#define SHLWAPI "Shlwapi.dll"
#else
#define ENVPATH_SEPARATORC ':'
#define ENVPATH_SEPARATOR ":"
#endif

#define CONFIG_SEPARATOR ","

#define BUILD_API_FILE "build/build.bl"
#define BUILTIN_FILE "builtin/a.bl"

// NOTE: We are converting all backslashes on Windows to the forward slashes.
#define PATH_SEPARATOR "/"
#define PATH_SEPARATORC '/'

#ifdef BL_DEBUG
#define ASSERT_ON_CMP_ERROR 0
#define BL_MAGIC_ENABLE 1
#else
#define ASSERT_ON_CMP_ERROR 0
#define BL_MAGIC_ENABLE 0
#endif

#define VM_STACK_SIZE 2097152              // 2MB
#define VM_COMPTIME_CALL_STACK_SIZE 131072 // 128kB

#define MODULE_CONFIG_FILE "module.yaml"
#define BUILD_SCRIPT_FILE "build.bl"

#if BL_COMPILER_CLANG || BL_COMPILER_GNUC

//
// FROM common.h
//

// clang-format off
#define _SHUT_UP_BEGIN \
_Pragma("GCC diagnostic push") \
_Pragma("GCC diagnostic ignored \"-Wcast-qual\"") \
_Pragma("GCC diagnostic ignored \"-Wpedantic\"") \
_Pragma("GCC diagnostic ignored \"-Wsign-conversion\"")
// clang-format on

#define _SHUT_UP_END _Pragma("GCC diagnostic pop")
#define UNUSED(x) __attribute__((unused)) x
#define _Thread_local __thread

#elif BL_COMPILER_MSVC

#define _SHUT_UP_BEGIN __pragma(warning(push, 0))
#define _SHUT_UP_END __pragma(warning(pop))
#define UNUSED(x) __pragma(warning(suppress : 4100)) x
#define _Thread_local __declspec(thread)

#else
#error "Unsuported compiler!"
#endif

//
// FROM bldebug.h
//

#define zone()               \
	TracyCZone(_tctx, true); \
	(void)0

#define _BL_VARGS(...) __VA_ARGS__
#define return_zone(...)                                    \
	{                                                       \
		TracyCZoneEnd(_tctx) return _BL_VARGS(__VA_ARGS__); \
	}                                                       \
	(void)0
