// =================================================================================================
// blc
//
// File:   wbs.c
// Author: Martin Dorazil
// Date:   12.19.21
//
// Copyright 2021 Martin Dorazil
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
// =================================================================================================

#ifdef BL_WBS
#error "This file is supposed to be included only once in unit!"
#endif

#include "builder.h"
#include "common.h"
#include "stb_ds.h"

#if !BL_PLATFORM_WIN
#error "Not compiling for Windows!"
#endif

#define BL_WBS

// Lookup various Windows bullshit and yes 'wbs' stands for 'windows bullshit'
#define BUILD_TOOLS "Microsoft Visual Studio/2019/BuildTools"
#define MSVC_TOOLS "VC/Tools/MSVC"
#define WIN_SDK "Windows Kits/10/Lib"
#define UCRT "ucrt/x64"
#define UM "um/x64"
#define MSVC_LIB "/lib/x64"

struct wbs {
	bool      is_valid;
	str_buf_t program_files_path;
	str_buf_t vs_path;
	str_buf_t windows_sdk_path;
	str_buf_t ucrt_path;
	str_buf_t um_path;
	str_buf_t msvc_lib_path;
};

// =================================================================================================
// PUBLIC API
// =================================================================================================
static struct wbs *wbslookup(void);
static void        wbsfree(struct wbs *wbs);

// =================================================================================================
// PRIVATE STUFF
// =================================================================================================
static bool _listdir(struct wbs *ctx, const str_buf_t dirpath, array(char *) * outdirs) {
	str_buf_t search = get_tmp_str();
	str_buf_append_fmt(&search, "{str}\\*", dirpath);
	WIN32_FIND_DATAA find_data;
	HANDLE           handle = FindFirstFileA(str_buf_to_c(search), &find_data);
	if (handle == INVALID_HANDLE_VALUE) {
		builder_error("Cannot list directory '%.*s'.", dirpath.len, dirpath.ptr);
		put_tmp_str(search);
		return false;
	}
	while (FindNextFileA(handle, &find_data) != 0) {
		if (find_data.cFileName[0] == '.') continue;
		char *tmp = strdup(find_data.cFileName);
		arrput((*outdirs), tmp);
	}
	FindClose(handle);
	put_tmp_str(search);
	return true;
}

static bool _lookup_program_files(struct wbs *ctx) {
	char *program_files_path = getenv("ProgramFiles(x86)");
	if (!program_files_path || !strlen(program_files_path)) {
		builder_error(
		    "The 'ProgramFiles(x86)' environment variable not found or not set properly!");
		return false;
	}

	str_buf_t dup = str_buf_dup(make_str_from_c(program_files_path));
	win_path_to_unix(str_buf_view(dup));
	ctx->program_files_path = dup;
	return true;
}

static bool _lookup_vs(struct wbs *ctx) {
	str_buf_t vspath = execute("vswhere.exe -latest -nologo -property installationPath");
	if (!vspath.len) {
		// try to use Build Tools instead!
		str_buf_append_fmt(&vspath, "{str}/{s}", ctx->program_files_path, BUILD_TOOLS);
		if (!dir_exists(vspath)) {
			builder_error("Visual Studio installation or MS Build Tools not found. Download & "
			              "install MS Build Tools from "
			              "https://visualstudio.microsoft.com/visual-cpp-build-tools. (Expected "
			              "location is '%.*s')",
			              vspath.len,
			              vspath.ptr);
			put_tmp_str(vspath);
			return false;
		}
	}
	win_path_to_unix(vspath);
	ctx->vs_path = str_buf_dup(vspath);
	put_tmp_str(vspath);
	return true;
}

static void _listfile_delete(char ***list) {
	for (usize i = 0; i < arrlenu(*list); ++i) {
		free((*list)[i]);
	}
	arrfree(*list);
}

static bool _lookup_windows_sdk(struct wbs *ctx) {
	str_buf_t sdkpath = {0};
	str_buf_append_fmt(&sdkpath, "{str}/{s}", ctx->program_files_path, WIN_SDK);
	if (!dir_exists(sdkpath)) {
		builder_error(
		    "Windows SDK not found. (Expected location is '%.*s')", sdkpath.len, sdkpath.ptr);
		str_buf_free(&sdkpath);
		return false;
	}
	array(char *) versions = NULL;
	if (!_listdir(ctx, sdkpath, &versions)) {
		_listfile_delete(&versions);
		str_buf_free(&sdkpath);
		return false;
	}
	s32   best[4]    = {0};
	s32   curr[4]    = {0};
	usize best_index = 0;
	for (usize i = 0; i < arrlenu(versions); ++i) {
		if (sscanf(versions[i], "%d.%d.%d.%d", &curr[0], &curr[1], &curr[2], &curr[3]) == 4) {
			if (curr[0] < best[0]) {
				continue;
			} else if (curr[0] == best[0]) {
				if (curr[1] < best[1]) {
					continue;
				} else if (curr[1] == best[1]) {
					if (curr[2] < best[2]) {
						continue;
					} else if (curr[2] == best[2]) {
						if (curr[3] < best[3]) {
							continue;
						}
					}
				}
			}
			memcpy(&best, &curr, static_arrlenu(best));
			best_index = i;
		}
	}

	str_buf_append_fmt(&sdkpath, "/{s}", versions[best_index]);
	_listfile_delete(&versions);
	ctx->windows_sdk_path = sdkpath;
	return true;
}

static bool _lookup_msvc_libs(struct wbs *ctx) {
	str_buf_t sdkpath = {0};
	str_buf_append_fmt(&sdkpath, "{str}/{s}", ctx->vs_path, MSVC_TOOLS);
	if (!dir_exists(sdkpath)) {
		builder_error(
		    "MSVC build tools not found. (Expected location is '%.*s')", sdkpath.len, sdkpath.ptr);
		str_buf_free(&sdkpath);
		return false;
	}
	char **versions = NULL;
	if (!_listdir(ctx, sdkpath, &versions)) {
		_listfile_delete(&versions);
		str_buf_free(&sdkpath);
		return false;
	}
	s32   best[3]    = {0};
	s32   curr[3]    = {0};
	usize best_index = 0;
	for (usize i = 0; i < arrlenu(versions); ++i) {
		if (sscanf(versions[i], "%d.%d.%d", &curr[0], &curr[1], &curr[2]) == 3) {
			if (curr[0] < best[0]) {
				continue;
			} else if (curr[0] == best[0]) {
				if (curr[1] < best[1]) {
					continue;
				} else if (curr[1] == best[1]) {
					if (curr[2] < best[2]) {
						continue;
					}
				}
			}
			memcpy(&best, &curr, static_arrlenu(best));
			best_index = i;
		}
	}

	str_buf_append_fmt(&sdkpath, "/{s}/{s}", versions[best_index], MSVC_LIB);
	_listfile_delete(&versions);
	if (!dir_exists(sdkpath)) {
		builder_error("MSVC lib directory not found. (Expected location is '%.*s')",
		              sdkpath.len,
		              sdkpath.ptr);
		str_buf_free(&sdkpath);
		return false;
	}
	ctx->msvc_lib_path = sdkpath;
	return true;
}

static bool _lookup_ucrt(struct wbs *ctx) {
	str_buf_t tmppath = {0};
	str_buf_append_fmt(&tmppath, "{str}/{s}", ctx->windows_sdk_path, UCRT);
	if (!dir_exists(tmppath)) {
		builder_error(
		    "UCRT lib-path not found. (Expected location is '%.*s')", tmppath.len, tmppath.ptr);
		str_buf_free(&tmppath);
		return false;
	}
	ctx->ucrt_path = tmppath;
	return true;
}

static bool _lookup_um(struct wbs *ctx) {
	str_buf_t tmppath = {0};
	str_buf_append_fmt(&tmppath, "{str}/{s}", ctx->windows_sdk_path, UM);
	if (!dir_exists(tmppath)) {
		builder_error(
		    "UM lib-path not found. (Expected location is '%.*s')", tmppath.len, tmppath.ptr);
		str_buf_free(&tmppath);
		return false;
	}
	ctx->um_path = tmppath;
	return true;
}

struct wbs *wbslookup(void) {
	struct wbs *wbs = bmalloc(sizeof(struct wbs));
	memset(wbs, 0, sizeof(struct wbs));
	if (!_lookup_program_files(wbs)) goto FAILED;
	if (!_lookup_vs(wbs)) goto FAILED;
	if (!_lookup_windows_sdk(wbs)) goto FAILED;
	if (!_lookup_ucrt(wbs)) goto FAILED;
	if (!_lookup_um(wbs)) goto FAILED;
	if (!_lookup_msvc_libs(wbs)) goto FAILED;

	wbs->is_valid = true;
FAILED:
	return wbs;
}

void wbsfree(struct wbs *wbs) {
	if (!wbs) return;
	str_buf_free(&wbs->msvc_lib_path);
	str_buf_free(&wbs->program_files_path);
	str_buf_free(&wbs->ucrt_path);
	str_buf_free(&wbs->um_path);
	str_buf_free(&wbs->vs_path);
	str_buf_free(&wbs->windows_sdk_path);
	bfree(wbs);
}
