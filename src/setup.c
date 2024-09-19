// =================================================================================================
// blc
//
// File:   setup.c
// Author: Martin Dorazil
// Date:   23/12/2021
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

#include "builder.h"
#include "common.h"
#include "stb_ds.h"

#if BL_PLATFORM_WIN
#include "wbs.h"
#endif

struct context {
	const char *triple;
	str_t       filepath;

	str_t version;
	str_t lib_dir;
	str_t preload_file;
	str_t linker_opt_exec;
	str_t linker_opt_shared;
	str_t linker_lib_path;
	str_t linker_executable;

	struct string_cache *cache;
};

static bool default_config(struct context *ctx);
static bool x86_64_pc_windows_msvc(struct context *ctx);
static bool x86_64_pc_linux_gnu(struct context *ctx);
static bool x86_64_apple_darwin(struct context *ctx);
static bool arm64_apple_darwin(struct context *ctx);

static str_buf_t make_content(const struct context *ctx);

bool setup(const str_t filepath, const char *triple) {
	struct context ctx    = {0};
	ctx.triple            = triple;
	ctx.filepath          = filepath;
	ctx.linker_executable = str_empty;
	ctx.linker_opt_exec   = str_empty;
	ctx.linker_opt_shared = str_empty;
	ctx.linker_lib_path   = str_empty;
	ctx.preload_file      = str_empty;

	bool state = false;

	// common config
	ctx.version      = make_str_from_c(BL_VERSION);
	str_buf_t libdir = get_tmp_str();
	str_buf_append_fmt(&libdir, "{str}/{s}", builder_get_exec_dir(), BL_API_DIR);
	if (!normalize_path(&libdir)) {
		builder_error("BL API directory not found. (Expected location is '" STR_FMT "').", STR_ARG(libdir));
		put_tmp_str(libdir);
		return false;
	}
	ctx.lib_dir = scprint(&ctx.cache, "{str}", libdir);
	put_tmp_str(libdir);

	if (strcmp(ctx.triple, "x86_64-pc-windows-msvc") == 0) {
#ifdef BL_WBS
		state = x86_64_pc_windows_msvc(&ctx);
#else
		state = default_config(&ctx);
#endif
	} else if (strcmp(ctx.triple, "x86_64-pc-linux-gnu") == 0 || strcmp(ctx.triple, "x86_64-unknown-linux-gnu") == 0) {
		state = x86_64_pc_linux_gnu(&ctx);
	} else if (strcmp(ctx.triple, "x86_64-apple-darwin") == 0) {
		state = x86_64_apple_darwin(&ctx);
	} else if (strcmp(ctx.triple, "arm64-apple-darwin") == 0) {
		state = arm64_apple_darwin(&ctx);
	} else {
		state = default_config(&ctx);
	}
	if (!state) {
		builder_error("Generate new configuration file at '" STR_FMT "' for target triple '%s' failed!", STR_ARG(ctx.filepath), ctx.triple);
		return false;
	}
	str_buf_t content = make_content(&ctx);
	if (file_exists(ctx.filepath)) { // backup old-one
		str_buf_t bakfilepath = get_tmp_str();
		char      date[26];
		date_time(date, static_arrlenu(date), "%d-%m-%Y_%H-%M-%S");
		str_buf_append_fmt(&bakfilepath, "{str}.{s}", ctx.filepath, date);
		builder_warning("Creating backup of previous configuration file at '" STR_FMT "'.", STR_ARG(bakfilepath));
		copy_file(ctx.filepath, str_buf_view(bakfilepath));
		put_tmp_str(bakfilepath);
	}

	const str_t dirpath = get_dir_from_filepath(ctx.filepath);
	if (!dir_exists(dirpath)) {
		if (!create_dir_tree(dirpath)) {
			builder_error("Cannot create directory path '" STR_FMT "'!", STR_ARG(dirpath));
			put_tmp_str(content);
			return false;
		}
	}

	str_buf_t tmp_filepath = get_tmp_str();
	FILE     *file         = fopen(str_dup_if_not_terminated(&tmp_filepath, ctx.filepath.ptr, ctx.filepath.len), "w");
	if (!file) {
		builder_error("Cannot open file '" STR_FMT "' for writing!", STR_ARG(ctx.filepath));
		put_tmp_str(content);
		put_tmp_str(tmp_filepath);
		return false;
	}
	put_tmp_str(tmp_filepath);
	fputs(str_buf_to_c(content), file);
	fclose(file);

	put_tmp_str(content);
	scfree(&ctx.cache);
	return true;
}

str_buf_t make_content(const struct context *ctx) {
#define TEMPLATE                                                                                \
	"# Automatically generated configuration file used by 'blc' compiler.\n"                    \
	"# To generate new one use 'blc --configure' command.\n\n"                                  \
	"# Compiler version, this should match the executable version 'blc --version'.\n"           \
	"version: \"{str}\"\n\n"                                                                    \
	"# Main API directory containing all modules and source files. This option is mandatory.\n" \
	"lib_dir: \"{str}\"\n"                                                                      \
	"\n"                                                                                        \
	"# Current default environment configuration.\n"                                            \
	"{s}:\n"                                                                                    \
	"    # Platform operating system preload file (relative to 'lib_dir').\n"                   \
	"    preload_file: \"{str}\"\n"                                                             \
	"    # Optional path to the linker executable, 'lld' linker is used by default on some "    \
	"platforms.\n"                                                                              \
	"    linker_executable: \"{str}\"\n"                                                        \
	"    # Linker flags and options used to produce executable binaries.\n"                     \
	"    linker_opt_exec: \"{str}\"\n"                                                          \
	"    # Linker flags and options used to produce shared libraries.\n"                        \
	"    linker_opt_shared: \"{str}\"\n"                                                        \
	"    # File system location where linker should lookup for dependencies.\n"                 \
	"    linker_lib_path: \"{str}\"\n\n"

	str_buf_t tmp = get_tmp_str();
	str_buf_append_fmt(&tmp,
	                   TEMPLATE,
	                   ctx->version,
	                   ctx->lib_dir,
	                   ctx->triple,
	                   ctx->preload_file,
	                   ctx->linker_executable,
	                   ctx->linker_opt_exec,
	                   ctx->linker_opt_shared,
	                   ctx->linker_lib_path);
	return tmp;

#undef TEMPLATE
}

// =================================================================================================
// Targets
// =================================================================================================
bool default_config(struct context UNUSED(*ctx)) {
	builder_warning("Automatic generation of configuration file is not supported for target triple "
	                "'%s' empty file will be generated at '" STR_FMT "'.",
	                ctx->triple,
	                STR_ARG(ctx->filepath));
	return true;
}

#ifdef BL_WBS
bool x86_64_pc_windows_msvc(struct context *ctx) {
	ctx->preload_file      = cstr("os/_windows.bl");
	ctx->linker_executable = str_empty;
	ctx->linker_opt_exec   = cstr("/NOLOGO /ENTRY:__os_start /SUBSYSTEM:CONSOLE /INCREMENTAL:NO /MACHINE:x64");
	ctx->linker_opt_shared = cstr("/NOLOGO /INCREMENTAL:NO /MACHINE:x64 /DLL");

	struct wbs *wbs = wbslookup();
	if (!wbs->is_valid) {
		builder_error("Configuration failed!");
		goto FAILED;
	}
	ctx->linker_lib_path =
	    scprint(&ctx->cache, "{str};{str};{str}", wbs->ucrt_path, wbs->um_path, wbs->msvc_lib_path);

	wbsfree(wbs);
	return true;
FAILED:
	wbsfree(wbs);
	return false;
}
#endif

bool x86_64_pc_linux_gnu(struct context *ctx) {
	const str_t RUNTIME_PATH      = cstr("lib/bl/rt/blrt_x86_64_linux.o");
	const str_t LINKER_OPT_EXEC   = cstr("-dynamic-linker /lib64/ld-linux-x86-64.so.2 -e _start");
	const str_t LINKER_OPT_SHARED = cstr("--shared");

	const str_t LINKER_LIB_PATHS[] = {
	    cstr("/lib"),
	    cstr("/lib64"),
	    cstr("/usr/lib"),
	    cstr("/usr/lib64"),
	    cstr("/usr/local/lib"),
	    cstr("/usr/local/lib64"),
	    cstr("/usr/lib/x86_64-linux-gnu"),
	    // Extend this in case we have some more known locations...
	};

	ctx->preload_file = cstr("os/_linux.bl");

	str_buf_t ldpath = execute("which ld");
	if (ldpath.len == 0) {
		builder_error("The 'ld' linker not found on the system!");
	}
	ctx->linker_executable = scdup2(&ctx->cache, ldpath);
	put_tmp_str(ldpath);

	str_buf_t runtime = get_tmp_str();
	str_buf_append_fmt(&runtime, "{str}/../{str}", builder_get_exec_dir(), RUNTIME_PATH);
	if (!normalize_path(&runtime)) {
		builder_error("Runtime loader not found. (Expected location is '" STR_FMT "').", STR_ARG(runtime));
		put_tmp_str(runtime);
		return false;
	}

	// Lookup all possible lib paths on the system.
	str_buf_t lib_paths = get_tmp_str();
	for (s32 i = 0; i < static_arrlenu(LINKER_LIB_PATHS); ++i) {
		const str_t path = LINKER_LIB_PATHS[i];
		if (file_exists(path)) {
			if (lib_paths.len > 0) {
				str_buf_append(&lib_paths, cstr(":"));
			}
			str_buf_append(&lib_paths, path);
		}
	}

	ctx->linker_opt_exec   = scprint(&ctx->cache, "{str} {str}", runtime, LINKER_OPT_EXEC);
	ctx->linker_opt_shared = scprint(&ctx->cache, "{str}", LINKER_OPT_SHARED);
	ctx->linker_lib_path   = scprint(&ctx->cache, "{str}", lib_paths);

	put_tmp_str(lib_paths);
	put_tmp_str(runtime);
	return true;
}

static bool x86_64_apple_darwin(struct context *ctx) {
	const str_t COMMAND_LINE_TOOLS = cstr("/Library/Developer/CommandLineTools");
	const str_t MACOS_SDK          = cstr("/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/lib");
	const str_t LINKER_LIB_PATH    = cstr("/usr/lib:/usr/local/lib");
	const str_t LINKER_OPT_EXEC    = cstr("-e ___os_start");
	const str_t LINKER_OPT_SHARED  = cstr("-dylib");

	ctx->preload_file = cstr("os/_macos.bl");

	if (!dir_exists(COMMAND_LINE_TOOLS)) {
		builder_error("Cannot find Command Line Tools on '%s', use 'xcode-select --install'.", COMMAND_LINE_TOOLS.ptr);
		return false;
	}

	str_buf_t libpath   = get_tmp_str();
	str_buf_t optexec   = get_tmp_str();
	str_buf_t optshared = get_tmp_str();

	str_buf_append(&libpath, LINKER_LIB_PATH);
	str_buf_t osver = execute("sw_vers -productVersion");
	if (osver.len == 0) {
		builder_error("Cannot detect macOS product version!");
	} else {
		s32 major, minor, patch;
		major = minor = patch = 0;
		if (sscanf(str_buf_to_c(osver), "%d.%d.%d", &major, &minor, &patch) == 3) {
			if (major >= 11) {
				if (!dir_exists(MACOS_SDK)) {
					builder_error("Cannot find macOS SDK on '%.*s'.", MACOS_SDK.len, MACOS_SDK.ptr);
				} else {
					str_buf_append_fmt(&libpath, ":{str}", MACOS_SDK);
				}
			}
		}
		str_buf_append_fmt(&optexec, "-macosx_version_min {str} -sdk_version {str} ", osver, osver);
		str_buf_append_fmt(&optshared, "-macosx_version_min {str} -sdk_version {str} ", osver, osver);
	}

	str_buf_append_fmt(&optexec, "{str}", LINKER_OPT_EXEC);
	str_buf_append_fmt(&optshared, "{str}", LINKER_OPT_SHARED);

	ctx->linker_lib_path   = scdup2(&ctx->cache, libpath);
	ctx->linker_opt_exec   = scdup2(&ctx->cache, optexec);
	ctx->linker_opt_shared = scdup2(&ctx->cache, optshared);
	put_tmp_str(osver);
	put_tmp_str(optexec);
	put_tmp_str(optshared);
	put_tmp_str(libpath);

	str_buf_t ldpath = execute("which ld");
	if (ldpath.len == 0) {
		builder_error("The 'ld' linker not found on system!");
	}
	ctx->linker_executable = scdup2(&ctx->cache, ldpath);
	put_tmp_str(ldpath);

	return true;
}

static bool arm64_apple_darwin(struct context *ctx) {
	const str_t COMMAND_LINE_TOOLS = cstr("/Library/Developer/CommandLineTools");
	const str_t MACOS_SDK          = cstr("/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/lib");
	const str_t LINKER_OPT_EXEC    = cstr("-e ___os_start -arch arm64");
	const str_t LINKER_OPT_SHARED  = cstr("-dylib -arch arm64");

	const str_t LINKER_LIB_PATHS[] = {
	    cstr("/usr/lib"),
	    cstr("/usr/local/lib"),
	    // Extend this in case we have some more known locations...
	};

	ctx->preload_file = cstr("os/_macos.bl");

	if (!dir_exists(COMMAND_LINE_TOOLS)) {
		builder_error("Cannot find Command Line Tools on '" STR_FMT "', use 'xcode-select --install'.", STR_ARG(COMMAND_LINE_TOOLS));
		return false;
	}

	str_buf_t libpath   = get_tmp_str();
	str_buf_t optexec   = get_tmp_str();
	str_buf_t optshared = get_tmp_str();

	// Lookup all possible lib paths on the system.
	for (s32 i = 0; i < static_arrlenu(LINKER_LIB_PATHS); ++i) {
		const str_t path = LINKER_LIB_PATHS[i];
		if (file_exists(path)) {
			if (libpath.len > 0) {
				str_buf_append(&libpath, cstr(":"));
			}
			str_buf_append(&libpath, path);
		}
	}

	str_buf_t osver = execute("sw_vers -productVersion");
	if (osver.len == 0) {
		builder_warning("Cannot detect macOS product version!");
	} else {
		s32 major, minor, patch;
		major = minor = patch = 0;
		if (sscanf(osver.ptr, "%d.%d.%d", &major, &minor, &patch) > 0) {
			// @Note 2024-08-04: minor and patch numbers are optional in the received string. We assume 0 in case they are
			// not provided... (We love apples...)
			if (major >= 11) {
				if (!dir_exists(MACOS_SDK)) {
					builder_error("Cannot find macOS SDK on '" STR_FMT "'.", STR_ARG(MACOS_SDK));
				} else {
					str_buf_append_fmt(&libpath, ":{str}", MACOS_SDK);
				}
			}
			// @Note 2024-08-04: This is a bit hack, newer macos versions does not have separated standard
			// libraries (probably to make developers constantly playing hide and seek with each version of
			// their shit). Since there is no concept of system version in module setup, we cannot set it
			// conditionally in the libc module :/ this is lame... This might be solved by the new system of
			// modules using BL executables for configuration.
			if (major < 14) {
				str_buf_append_fmt(&optexec, "-lc -lm ");
				str_buf_append_fmt(&optshared, "-lc -lm ");
			} else {
				str_buf_append_fmt(&optexec, "-lSystem ");
				str_buf_append_fmt(&optshared, "-lSystem ");
			}
		} else {
			builder_warning("Cannot parse macOS product version, provided string is '" STR_FMT "'!", STR_ARG(osver));
		}
		str_buf_append_fmt(&optexec, "-macos_version_min {str} ", osver);
		str_buf_append_fmt(&optshared, "-macos_version_min {str} ", osver);
	}

	str_buf_append(&optexec, LINKER_OPT_EXEC);
	str_buf_append(&optshared, LINKER_OPT_SHARED);

	ctx->linker_lib_path   = scdup2(&ctx->cache, libpath);
	ctx->linker_opt_exec   = scdup2(&ctx->cache, optexec);
	ctx->linker_opt_shared = scdup2(&ctx->cache, optshared);
	put_tmp_str(osver);
	put_tmp_str(optexec);
	put_tmp_str(optshared);
	put_tmp_str(libpath);

	str_buf_t ldpath = execute("which ld");
	if (ldpath.len == 0) {
		builder_error("The 'ld' linker not found on system!");
	}
	ctx->linker_executable = scdup2(&ctx->cache, ldpath);
	put_tmp_str(ldpath);

	return true;
}
