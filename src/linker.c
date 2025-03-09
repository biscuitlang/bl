#include <ctype.h>
#include "builder.h"
#include "common.h"
#include "conf.h"
#include "stb_ds.h"

#define link_error(code, tok, pos, format, ...)                                             \
	{                                                                                       \
		if (tok)                                                                            \
			builder_msg(MSG_ERR, (code), &(tok)->location, (pos), (format), ##__VA_ARGS__); \
		else                                                                                \
			builder_error((format), ##__VA_ARGS__);                                         \
	}                                                                                       \
	(void)0

struct context {
	struct assembly *assembly;
};

#if BL_PLATFORM_LINUX

//
// @Note 2025-03-09
//
// On linux systems, the dynamic library may point to the thing so called LD script; which is essentially
// text file containing mapping to real libraries.
//
// Here we try to detect if pointed file is actual ELF binary, in case it's not, we assume it's LD script
// and try to parse it.
//
// Note that we currently take only the first ELF binary found in the LD script file (there might be more
// of them). Also we assume the first ELF file is dynamic library (which might not be true).
//
// Improve this in future!
//

#define ELF_MAGIC "\x7f" \
	              "ELF"

static FILE *try_open_ld_script(const str_t filepath) {
	str_buf_t tmp  = get_tmp_str();
	FILE     *file = fopen(str_to_c(&tmp, filepath), "rb");
	put_tmp_str(tmp);

	if (!file) {
		builder_log("  Could not open: '" STR_FMT "'", STR_ARG(filepath));
		return NULL;
	}
	char buffer[4] = {0};
	if (fread(buffer, 1, 4, file) != 4) {
		builder_log("  Could not read whole magic header: '" STR_FMT "'", STR_ARG(filepath));
		fclose(file);
		return NULL;
	}
	if (memcmp(buffer, ELF_MAGIC, 4) == 0) {
		fclose(file);
		return NULL;
	}
	fseek(file, 0, SEEK_SET);
	return file;
}

static bool is_elf(const str_t filepath) {
	FILE *file = try_open_ld_script(filepath);
	if (file) {
		fclose(file);
		return false;
	}
	return true;
}

static bool parse_ld_script(FILE *file, str_buf_t *filepath) {
#define MAX_LINE 1024

	char line[MAX_LINE];
	while (fgets(line, sizeof(line), file)) {
		if (strstr(line, "GROUP") || strstr(line, "INPUT")) {
			char *p = strchr(line, '(');
			if (!p) continue;
			p++;

			while (*p) {
				while (isspace((unsigned char)*p))
					p++;
				if (*p == ')') break; // End of group

				char path[MAX_LINE];
				s32  len = 0;
				while (*p && !isspace((unsigned char)*p) && *p != ')') {
					path[len++] = *p++;
				}

				if (is_elf(make_str(path, len))) {
					str_buf_clr(filepath);
					str_buf_append(filepath, make_str(path, len));
					return true;
				}
			}
		}
	}
	return false;

#undef MAX_LINE
}

#endif

static bool search_library(struct context *ctx,
                           str_t           lib_name,
                           str_t          *out_lib_name,
                           str_t          *out_lib_dir,
                           str_t          *out_lib_filepath) {
	bool found = false;

	const u32             thread_index = get_worker_index();
	struct string_cache **string_cache = &ctx->assembly->thread_local_contexts[thread_index].string_cache;

	str_buf_t lib_filepath      = get_tmp_str();
	str_buf_t lib_platform_name = platform_lib_name(lib_name);

	builder_log("- Looking for: '" STR_FMT "'", STR_ARG(lib_platform_name));
	for (usize i = 0; i < arrlenu(ctx->assembly->lib_paths); ++i) {
		char *dir = ctx->assembly->lib_paths[i];
		builder_log("- Search in: '%s'", dir);

		str_buf_clr(&lib_filepath);
		str_buf_append_fmt(&lib_filepath, "{s}/{str}", dir, lib_platform_name);

		if (normalize_path(&lib_filepath)) {
			builder_log("  Found: '" STR_FMT "'", STR_ARG(lib_filepath));

#if BL_PLATFORM_LINUX
			FILE *ld_script = try_open_ld_script(str_buf_view(lib_filepath));
			if (ld_script) {
				builder_log("  Loading LD script: '" STR_FMT "'", STR_ARG(lib_filepath));

				if (!parse_ld_script(ld_script, &lib_filepath)) {
					builder_log("  Unable to parse LD script: '" STR_FMT "'", STR_ARG(lib_filepath));
					fclose(ld_script);
					continue;
				}
				fclose(ld_script);

				builder_log("  Redirected to: '" STR_FMT "'", STR_ARG(lib_filepath));
			}

#endif

			if (out_lib_name) {
				(*out_lib_name) = scdup2(string_cache, get_filename_from_filepath(lib_filepath));
			}
			if (out_lib_dir) {
				(*out_lib_dir) = scdup2(string_cache, get_dir_from_filepath(lib_filepath));
			}
			if (out_lib_filepath) {
				(*out_lib_filepath) = scdup2(string_cache, lib_filepath);
			}
			found = true;
			goto DONE;
		}
	}

DONE:
	if (!found) builder_log("  Not loaded: '" STR_FMT "'", STR_ARG(lib_filepath));
	put_tmp_str(lib_platform_name);
	put_tmp_str(lib_filepath);

	return found;
}

void add_lib_path(struct context *ctx, const char *token) {
	if (file_exists(make_str_from_c(token))) {
		char *path = strdup(token);
#if BL_PLATFORM_WIN
		win_path_to_unix(make_str_from_c(path));
#endif
		arrput(ctx->assembly->lib_paths, path);
	} else {
		builder_warning("Invalid LIB_PATH entry value '%s'.", token);
	}
}

static void set_lib_paths(struct context *ctx) {
	const char *lib_path = read_config(builder.config, ctx->assembly->target, "linker_lib_path", "");
	if (!strlen(lib_path)) return;
	process_tokens(ctx, lib_path, ENVPATH_SEPARATOR, (process_tokens_fn_t)&add_lib_path);
}

static bool link_lib(struct context *ctx, struct native_lib *lib) {
	if (!lib) babort("invalid lib");
	if (!lib->user_name.len) babort("invalid lib name");
	if (!search_library(ctx, lib->user_name, &lib->filename, &lib->dir, &lib->filepath)) {
		return false;
	}
	if ((lib->flags & NATIVE_LIB_FLAG_COMPTIME) == 0) {
		builder_log("- Library without 'NATIVE_LIB_FLAG_COMPTIME' flag '" STR_FMT "' skipped.", STR_ARG(lib->user_name));
		return true;
	}
	str_buf_t tmp_path = get_tmp_str();
	lib->handle        = dlLoadLibrary(str_to_c(&tmp_path, lib->filepath));
	put_tmp_str(tmp_path);
	return lib->handle;
}

static bool link_working_environment(struct context *ctx, const char *lib_name) {
	DLLib *handle = dlLoadLibrary(lib_name);
	if (!handle) return false;

	struct native_lib native_lib = {0};
	native_lib.handle            = handle;
	native_lib.flags |= NATIVE_LIB_FLAG_COMPTIME;

	arrput(ctx->assembly->libs, native_lib);
	return true;
}

void linker_run(struct assembly *assembly) {
	zone();
	struct context ctx;
	ctx.assembly = assembly;
	builder_log("Running compile-time linker...");
	set_lib_paths(&ctx);

	for (usize i = 0; i < arrlenu(assembly->libs); ++i) {
		struct native_lib *lib = &assembly->libs[i];
		if (!link_lib(&ctx, lib)) {
			char      error_buffer[256];
			const s32 error_len = get_last_error(error_buffer, static_arrlenu(error_buffer));
			link_error(ERR_LIB_NOT_FOUND,
			           lib->linked_from,
			           CARET_WORD,
			           "Cannot load library '" STR_FMT "' with error: %s",
			           STR_ARG(lib->user_name),
			           error_len ? error_buffer : "UNKNOWN");
		}
	}

#if BL_PLATFORM_WIN
	if (!link_working_environment(&ctx, MSVC_CRT)) {
		struct token *dummy = NULL;
		link_error(ERR_LIB_NOT_FOUND, dummy, CARET_WORD, "Cannot link " MSVC_CRT);
		return_zone();
	}
	if (!link_working_environment(&ctx, KERNEL32)) {
		struct token *dummy = NULL;
		link_error(ERR_LIB_NOT_FOUND, dummy, CARET_WORD, "Cannot link " KERNEL32);
		return_zone();
	}
	if (!link_working_environment(&ctx, SHLWAPI)) {
		struct token *dummy = NULL;
		link_error(ERR_LIB_NOT_FOUND, dummy, CARET_WORD, "Cannot link " SHLWAPI);
		return_zone();
	}
#endif
	if (!link_working_environment(&ctx, NULL)) {
		struct token *dummy = NULL;
		link_error(ERR_LIB_NOT_FOUND, dummy, CARET_WORD, "Cannot link working environment.");
		return_zone();
	}
	return_zone();
}
