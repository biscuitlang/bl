// =================================================================================================
// STB
// =================================================================================================
#include "bldebug.h"
#include "blmemory.h"
#include "builder.h"
#include "conf.h"
#define STB_DS_IMPLEMENTATION
#define STBDS_REALLOC(context, ptr, size) brealloc(ptr, size)
#define STBDS_FREE(context, ptr)          bfree(ptr)
#include "stb_ds.h"

#include "assembly.h"
#include "builder.h"
#include "common.h"
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#ifdef BL_USE_SIMD
#include <emmintrin.h>
#include <intrin.h>
#include <nmmintrin.h>
#endif

#if !BL_PLATFORM_WIN
#include <sys/stat.h>
#include <unistd.h>
#endif

#if BL_PLATFORM_MACOS
#include <ctype.h>
#include <errno.h>
#include <mach-o/dyld.h>
#include <mach/mach_time.h>
#include <stdlib.h>
#include <sys/time.h>
#endif

#if BL_PLATFORM_LINUX
#include <ctype.h>
#include <errno.h>
#endif

#if BL_PLATFORM_WIN
#include <shlwapi.h>
#ifndef popen
#define popen _popen
#endif
#ifndef pclose
#define pclose _pclose
#endif
#endif

// =================================================================================================
// PUBLIC
// =================================================================================================

// =================================================================================================
// Small Array
// =================================================================================================

void sarradd_impl(void *ptr, usize elem_size, usize static_elem_count, usize new_elem_count) {
	if (new_elem_count < 1) return;
	sarr_any_t *arr        = (sarr_any_t *)ptr;
	const bool  on_heap    = arr->cap;
	const usize needed_len = arr->len + new_elem_count;
	if (on_heap && needed_len > arr->cap) {
		arr->cap  = (u32)(arr->cap * 2 > needed_len ? arr->cap * 2 : needed_len);
		void *tmp = arr->_data;
		if ((arr->_data = brealloc(arr->_data, arr->cap * elem_size)) == NULL) {
			bfree(tmp);
			abort();
		}
	} else if (!on_heap && needed_len > static_elem_count) {
		arr->cap   = (u32)(static_elem_count * 2 > needed_len ? static_elem_count * 2 : needed_len);
		void *data = bmalloc(arr->cap * elem_size);
		memcpy(data, arr->_buf, elem_size * arr->len);
		arr->_data = data;
	}
	arr->len += (u32)new_elem_count;
}

// =================================================================================================
// String cache
// =================================================================================================
#define SC_BLOCK_BYTES 2048

struct string_cache {
	u32 len;
	u32 cap;

	struct string_cache *prev;
};

static struct string_cache *new_block(usize len, struct string_cache *prev) {
	zone();
	const usize          cap   = len > SC_BLOCK_BYTES ? len : SC_BLOCK_BYTES;
	struct string_cache *cache = bmalloc(sizeof(struct string_cache) + cap);
	cache->cap                 = (u32)cap;
	cache->prev                = prev;
	cache->len                 = 0;
	return_zone(cache);
}

char *scdup(struct string_cache **cache, const char *str, usize len) {
	zone();
	len += 1; // +zero terminator
	if (!*cache) {
		(*cache) = new_block(len, NULL);
	} else if ((*cache)->len + len >= (*cache)->cap) {
		(*cache) = new_block(len, *cache);
	}
	char *mem = ((char *)((*cache) + 1)) + (*cache)->len;
	if (str) {
		memcpy(mem, str, len - 1); // Do not copy zero terminator.
		mem[len - 1] = '\0';       // Set zero terminator.
	}
	(*cache)->len += (u32)len;
	return_zone(mem);
}

str_t _scdup2(struct string_cache **cache, char *str, s32 len) {
	len += 1; // +zero terminator
	if (!*cache) {
		(*cache) = new_block(len, NULL);
	} else if ((*cache)->len + len >= (*cache)->cap) {
		(*cache) = new_block(len, *cache);
	}
	char *mem = ((char *)((*cache) + 1)) + (*cache)->len;
	if (str) {
		memcpy(mem, str, len - 1); // Do not copy zero terminator.
		mem[len - 1] = '\0';       // Set zero terminator.
	}
	(*cache)->len += (u32)len;
	return make_str(mem, len - 1); // -1 zero terminator!
}

void scfree(struct string_cache **cache) {
	struct string_cache *c = (*cache);
	while (c) {
		struct string_cache *prev = c->prev;
		bfree(c);
		c = prev;
	}
	(*cache) = NULL;
}

str_t scprint(struct string_cache **cache, const char *fmt, ...) {
	va_list args, args2;
	va_start(args, fmt);
	va_copy(args2, args);
	const s32 len = bvsnprint(NULL, 0, fmt, args);
	bassert(len > 0);
	char     *buf  = scdup(cache, NULL, len);
	const s32 wlen = bvsnprint(buf, len, fmt, args2);
	bassert(wlen == len);
	(void)wlen;
	va_end(args2);
	va_end(args);
	return make_str(buf, len);
}

// =================================================================================================
// String Buffer
// =================================================================================================

void str_buf_free(str_buf_t *buf) {
	bfree(buf->ptr);
	buf->cap = 0;
	buf->len = 0;
	buf->ptr = NULL;
}

void str_buf_clr(str_buf_t *buf) {
	buf->len = 0;
	if (buf->ptr && buf->cap) buf->ptr[0] = '\0';
}

void str_buf_setcap(str_buf_t *buf, s32 cap) {
	if (cap < 1) return;
	cap += 1; // This is for zero terminator.
	if (buf->cap >= cap) return;

	buf->ptr = brealloc(buf->ptr, MAX(cap, 64));
	buf->cap = cap;
}

void _str_buf_append(str_buf_t *buf, char *ptr, s32 len) {
	if (len == 0) return;
	bassert(ptr);
	if (buf->len + len >= buf->cap) {
		str_buf_setcap(buf, (buf->len + len) * 2);
	}
	memcpy(&buf->ptr[buf->len], ptr, len);
	buf->len += len;
	bassert(buf->len < buf->cap);
	buf->ptr[buf->len] = '\0';
}

void str_buf_append_fmt(str_buf_t *buf, const char *fmt, ...) {
	va_list args, args2;
	va_start(args, fmt);
	va_copy(args2, args);
	const s32 len = bvsnprint(NULL, 0, fmt, args);
	bassert(len > 0);

	str_buf_setcap(buf, buf->len + len);

	const s32 wlen = bvsnprint(&buf->ptr[buf->len], len, fmt, args2);
	bassert(wlen == len);
	(void)wlen;

	buf->len += len;

	bassert(buf->len < buf->cap);
	buf->ptr[buf->len] = '\0';

	va_end(args2);
	va_end(args);
}

s32 bvsnprint(char *buf, s32 buf_len, const char *fmt, va_list args) {
	const char *i         = fmt;
	s32         buf_index = 0;
	char        tmp[64];

	while (*i != '\0') {
		if (buf && buf_index >= buf_len) break;
		if (*i != '{' && *i != '}') {
			if (buf) buf[buf_index] = *i;
			++buf_index;
			++i;
			continue;
		}
		++i;

		if (*i == '{' || *i == '}') {
			if (buf) buf[buf_index] = *i;
			++buf_index;
			++i;
			continue;
		}

		const s32 space_left = buf_len - buf_index;

		str_t f = make_str(i, 3);
		if (str_match(f, cstr("str"))) {
			const str_t s = va_arg(args, str_t);
			if (buf) {
				const s32 len = MIN(space_left, s.len);
				memcpy(&buf[buf_index], s.ptr, len);
				buf_index += len;
			} else {
				buf_index += s.len;
			}

			i += f.len;
			goto PASSED;
		}

		if (str_match(f, cstr("s32"))) {
			const s32 s       = va_arg(args, s32);
			const s32 tmp_len = snprintf(tmp, static_arrlenu(tmp), "%i", s);
			if (buf) {
				const s32 len = MIN(space_left, tmp_len);
				memcpy(&buf[buf_index], tmp, len);
				buf_index += len;
			} else {
				buf_index += tmp_len;
			}

			i += f.len;
			goto PASSED;
		}

		if (str_match(f, cstr("u64"))) {
			const u64 s = va_arg(args, u64);
			if (buf) {
				const s32 tmp_len = snprintf(tmp, static_arrlenu(tmp), "%llu", s);
				const s32 len     = MIN(space_left, tmp_len);
				memcpy(&buf[buf_index], tmp, len);
				buf_index += len;
			} else {
				const s32 tmp_len = snprintf(NULL, 0, "%llu", s);
				buf_index += tmp_len;
			}

			i += f.len;
			goto PASSED;
		}

		if (str_match(f, cstr("s64"))) {
			const s64 s = va_arg(args, s64);
			if (buf) {
				const s32 tmp_len = snprintf(tmp, static_arrlenu(tmp), "%lli", s);
				const s32 len     = MIN(space_left, tmp_len);
				memcpy(&buf[buf_index], tmp, len);
				buf_index += len;
			} else {
				const s32 tmp_len = snprintf(NULL, 0, "%lli", s);
				buf_index += tmp_len;
			}

			i += f.len;
			goto PASSED;
		}

		if (str_match(f, cstr("u32"))) {
			const u32 s = va_arg(args, u32);
			if (buf) {
				const s32 tmp_len = snprintf(tmp, static_arrlenu(tmp), "%u", s);
				const s32 len     = MIN(space_left, tmp_len);
				memcpy(&buf[buf_index], tmp, len);
				buf_index += len;
			} else {
				const s32 tmp_len = snprintf(NULL, 0, "%u", s);
				buf_index += tmp_len;
			}

			i += f.len;
			goto PASSED;
		}

		switch (*i) {
		case 's': {
			char     *s     = va_arg(args, char *);
			const s32 s_len = (s32)strlen(s);
			if (buf) {
				const s32 len = MIN(space_left, s_len);
				memcpy(&buf[buf_index], s, len);
				buf_index += len;
			} else {
				buf_index += s_len;
			}

			i += 1;
			goto PASSED;
		}
		}

		babort("Invalid formating string!");

	PASSED:
		if (*i++ != '}') {
			babort("Expected end of formating sequence!");
		}
	}
	return buf_index;
}

s32 bsnprint(char *buf, s32 buf_len, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	const s32 c = bvsnprint(buf, buf_len, fmt, args);
	va_end(args);
	return c;
}

str_buf_t _str_buf_dup(char *ptr, s32 len) {
	str_buf_t result = {0};
	_str_buf_append(&result, ptr, len);
	return result;
}

// =================================================================================================
// Utils
// =================================================================================================

str_t str_toupper(str_t str) {
	for (s32 i = 0; i < str.len; ++i) {
		str.ptr[i] = toupper(str.ptr[i]);
	}
	return str;
}

bool str_match(str_t a, str_t b) {
	if (a.len != b.len) return false;

#ifdef BL_USE_SIMD
	__m128i *ita = (__m128i *)a.ptr;
	__m128i *itb = (__m128i *)b.ptr;

	const __m128i a16 = _mm_loadu_si128(ita);
	const __m128i b16 = _mm_loadu_si128(itb);

	if (_mm_cmpestrc(a16,
	                 a.len,
	                 b16,
	                 b.len,
	                 _SIDD_SBYTE_OPS | _SIDD_CMP_EQUAL_EACH | _SIDD_NEGATIVE_POLARITY |
	                     _SIDD_LEAST_SIGNIFICANT) != 0) {
		return false;
	}

	return true;
#else
	for (s64 i = 0; i < a.len; i += 1) {
		if (a.ptr[i] != b.ptr[i]) return false;
	}
	return true;
#endif
}

#define MIN3(a, b, c)          ((a) < (b) ? ((a) < (c) ? (a) : (c)) : ((b) < (c) ? (b) : (c)))
#define LEVENSHTEIN_MAX_LENGTH 256

// Compute Levenshtein distance of two strings (the legth of both string is limited to
// LEVENSHTEIN_MAX_LENGTH).
// Copy-paste from
// https://en.wikibooks.org/wiki/Algorithm_Implementation/Strings/Levenshtein_distance#C
s32 levenshtein(const str_t s1, const str_t s2) {
	u32   x, y, lastdiag, olddiag;
	usize s1len = MIN(s1.len, LEVENSHTEIN_MAX_LENGTH);
	usize s2len = MIN(s2.len, LEVENSHTEIN_MAX_LENGTH);
	u32   column[LEVENSHTEIN_MAX_LENGTH + 1];
	for (y = 1; y <= s1len; ++y)
		column[y] = y;
	for (x = 1; x <= s2len; ++x) {
		column[0] = x;
		for (y = 1, lastdiag = x - 1; y <= s1len; y++) {
			olddiag   = column[y];
			column[y] = MIN3(column[y] + 1,
			                 column[y - 1] + 1,
			                 lastdiag + (s1.ptr[y - 1] == s2.ptr[x - 1] ? 0 : 1));
			lastdiag  = olddiag;
		}
	}
	return column[s1len];
}

bool search_source_file(const str_t filepath, const str_t preferred_directory, str_buf_t *out_filepath) {
	str_buf_t tmp    = get_tmp_str();
	str_buf_t result = get_tmp_str(); // out_filepath can be null
	if (!filepath.len) goto NOT_FOUND;

	if (file_exists(filepath) && brealpath(filepath, &result)) goto FOUND;

	// Lookup in prefered directory if set.
	if (preferred_directory.len) {
		str_buf_append_fmt(&tmp, "{str}" PATH_SEPARATOR "{str}", preferred_directory, filepath);
		if (brealpath(tmp, &result)) {
			goto FOUND;
		}
	}

	// Search LIB_DIR.
	const str_t lib_dir = builder_get_lib_dir();
	if (lib_dir.len) {
		str_buf_clr(&tmp);
		str_buf_append_fmt(&tmp, "{str}" PATH_SEPARATOR "{str}", lib_dir, filepath);
		if (brealpath(tmp, &result)) {
			goto FOUND;
		}
	}

	// Search system PATH.
	{
		bool  found = false;
		char *env   = strdup(getenv(ENV_PATH));
		char *s     = env;
		char *p     = NULL;
		do {
			p = strchr(s, ENVPATH_SEPARATORC);
			if (p != NULL) {
				p[0] = 0;
			}
			str_buf_clr(&tmp);
			str_buf_append_fmt(&tmp, "{s}" PATH_SEPARATOR "{str}", s, filepath);
			found = brealpath(tmp, &result);
			s     = p + 1;
		} while (p && !found);
		free(env);
		if (found) goto FOUND;
	}

NOT_FOUND:
	put_tmp_str(tmp);
	put_tmp_str(result);
	return false;

FOUND:
	// Absolute file path.
	if (out_filepath) str_buf_append(out_filepath, result);
	put_tmp_str(tmp);
	put_tmp_str(result);
	return true;
}

void _win_path_to_unix(char *ptr, const s32 len) {
	for (s32 i = 0; i < len; ++i) {
		const char c = ptr[i];
		if (c != '\\') continue;
		ptr[i] = '/';
	}
}

void _unix_path_to_win(char *ptr, const s32 len) {
	for (usize i = 0; i < len; ++i) {
		const char c = ptr[i];
		if (c != '/') continue;
		ptr[i] = '\\';
	}
}

bool get_current_exec_path(str_buf_t *out_full_path) {
	bassert(out_full_path);
	bool result = false;
	char buf[PATH_MAX];
#if BL_PLATFORM_WIN
	result = GetModuleFileNameA(NULL, buf, PATH_MAX);
#elif BL_PLATFORM_LINUX
	result = readlink("/proc/self/exe", buf, PATH_MAX) >= 0;
#elif BL_PLATFORM_MACOS
	u32 buf_size = PATH_MAX;
	result       = _NSGetExecutablePath(buf, &buf_size) >= 0;
#else
#error Unsupported platform.
#endif

	if (result) {
		str_buf_clr(out_full_path);
#if BL_PLATFORM_MACOS
		char resolved[PATH_MAX];
		realpath(buf, resolved);
		str_buf_append(out_full_path, make_str_from_c(resolved));
#else
		str_buf_append(out_full_path, make_str_from_c(buf));
#endif
#if BL_PLATFORM_WIN
		win_path_to_unix(*out_full_path);
#endif
		return true;
	}
	return false;
}

bool get_current_exec_dir(str_buf_t *out_full_path) {
	if (!get_current_exec_path(out_full_path)) return false;
	out_full_path->len = get_dir_from_filepath(*out_full_path).len;
	return true;
}

bool set_current_working_dir(const char *path) {
#if BL_PLATFORM_WIN
	return SetCurrentDirectoryA(path);
#else
	return chdir(path) != -1;
#endif
}

bool _file_exists(char *ptr, const s32 len) {
	str_buf_t tmp_filepath = get_tmp_str();
	bool      result;
#if BL_PLATFORM_WIN
	result = (bool)PathFileExistsA(str_dup_if_not_terminated(&tmp_filepath, ptr, len));
#else
	struct stat tmp;
	result = stat(str_dup_if_not_terminated(&tmp_filepath, ptr, len), &tmp) == 0;
#endif
	put_tmp_str(tmp_filepath);
	return result;
}

bool _dir_exists(char *ptr, const s32 len) {
	str_buf_t tmp    = get_tmp_str();
	bool      result = false;
#if BL_PLATFORM_WIN
	DWORD dwAttrib = GetFileAttributesA(str_dup_if_not_terminated(&tmp, ptr, len));
	result         = dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY);
#else
	struct stat sb;
	result = stat(str_dup_if_not_terminated(&tmp, ptr, len), &sb) == 0 && S_ISDIR(sb.st_mode);
#endif
	put_tmp_str(tmp);
	return result;
}

#if BL_PLATFORM_WIN
bool _brealpath(char *ptr, const s32 len, str_buf_t *out_full_path) {
	bassert(out_full_path);
	if (!len) return false;

	str_buf_t   tmp_path = get_tmp_str();
	char        buf[PATH_MAX];
	bool        result   = false;
	const DWORD full_len = GetFullPathNameA(str_dup_if_not_terminated(&tmp_path, ptr, len), PATH_MAX, buf, NULL);
	if (!full_len) {
		DWORD ec = GetLastError();
		if (ec == ERROR_FILENAME_EXCED_RANGE) {
			builder_error("Cannot get full path of '%.*s', resulting path is too long. (expected maximum %d characters)", ptr, len, PATH_MAX);
		}
		goto DONE;
	}
	if (_file_exists(buf, full_len)) {
		str_buf_clr(out_full_path);
		str_buf_append(out_full_path, make_str(buf, full_len));
		win_path_to_unix(*out_full_path);
		result = true;
	}
DONE:
	put_tmp_str(tmp_path);
	return result;
}
#else
bool _brealpath(char *ptr, const s32 len, str_buf_t *out_full_path) {
	bassert(out_full_path);
	str_buf_t  tmp_path = get_tmp_str();
	char       buf[PATH_MAX];
	const bool result = realpath(str_dup_if_not_terminated(&tmp_path, ptr, len), buf);
	if (result) {
		str_buf_clr(out_full_path);
		str_buf_append(out_full_path, make_str_from_c(buf));
	}
	put_tmp_str(tmp_path);
	return result;
}
#endif

bool normalize_path(str_buf_t *path) {
	str_buf_t tmp = get_tmp_str();
	if (!brealpath(*path, &tmp)) {
		put_tmp_str(tmp);
		return false;
	}
	str_buf_clr(path);
	str_buf_append(path, tmp);
	put_tmp_str(tmp);
	return true;
}

bool _create_dir(char *ptr, const s32 len) {
	str_buf_t tmp_path = get_tmp_str();
	bool      result;
#if BL_PLATFORM_WIN
	result = CreateDirectoryA(str_dup_if_not_terminated(&tmp_path, ptr, len), NULL) != 0;
#else
	result = mkdir(str_dup_if_not_terminated(&tmp_path, ptr, len), 0700) == 0;
#endif
	put_tmp_str(tmp_path);
	return result;
}

bool create_dir_tree(const str_t dirpath) {
	str_buf_t tmp    = get_tmp_str();
	s32       prev_i = 0;
	for (s32 i = 0; i < dirpath.len; ++i) {
		if (dirpath.ptr[i] == PATH_SEPARATORC) {
			if (i - prev_i > 1) {
				if (!dir_exists(tmp)) {
					if (!create_dir(tmp)) {
						put_tmp_str(tmp);
						return false;
					}
				}
			}
			prev_i = i;
		}
		str_buf_append_char(&tmp, dirpath.ptr[i]);
	}
	if (!dir_exists(tmp)) {
		if (!create_dir(tmp)) {
			put_tmp_str(tmp);
			return false;
		}
	}
	put_tmp_str(tmp);
	return true;
}

bool copy_dir(const str_t src, const str_t dest) {
	str_buf_t tmp = get_tmp_str();
#if BL_PLATFORM_WIN
	str_buf_t tmp_src  = get_tmp_str_from(src);
	str_buf_t tmp_dest = get_tmp_str_from(dest);
	unix_path_to_win(tmp_src);
	unix_path_to_win(tmp_dest);
	str_buf_append_fmt(&tmp, "xcopy /H /E /Y /I \"{str}\" \"{str}\" 2>nul 1>nul", tmp_src, tmp_dest);
	put_tmp_str(tmp_src);
	put_tmp_str(tmp_dest);
#else
	str_buf_append_fmt(&tmp, "mkdir -p {str} && cp -rf {str}/* {str}", dest, src, dest);
#endif
	const bool result = system(str_buf_to_c(tmp)) == 0;
	put_tmp_str(tmp);
	return result;
}

bool copy_file(const str_t src, const str_t dest) {
	str_buf_t tmp = get_tmp_str();
#if BL_PLATFORM_WIN
	str_buf_t tmp_src  = get_tmp_str_from(src);
	str_buf_t tmp_dest = get_tmp_str_from(dest);
	unix_path_to_win(tmp_src);
	unix_path_to_win(tmp_dest);
	str_buf_append_fmt(&tmp, "copy /Y /B \"{str}\" \"{str}\" 2>nul 1>nul", tmp_src, tmp_dest);
	put_tmp_str(tmp_src);
	put_tmp_str(tmp_dest);
#else
	str_buf_append_fmt(&tmp, "cp -f {str} {str}", src, dest);
#endif
	const bool result = system(str_buf_to_c(tmp)) == 0;
	put_tmp_str(tmp);
	return result;
}

bool remove_dir(const str_t path) {
	str_buf_t tmp = get_tmp_str();
#if BL_PLATFORM_WIN
	str_buf_t tmp_path = get_tmp_str_from(path);
	unix_path_to_win(tmp_path);
	str_buf_append_fmt(&tmp, "del \"{str}\" /q /s 2>nul 1>nul", tmp_path);
	put_tmp_str(tmp_path);
#else
	str_buf_append_fmt(&tmp, "rm -rf {str}", path);
#endif
	const bool result = system(str_buf_to_c(tmp)) == 0;
	put_tmp_str(tmp);
	return result;
}
void date_time(char *buf, s32 len, const char *format) {
	bassert(buf && len);
	time_t     timer;
	struct tm *tm_info;
	time(&timer);
	tm_info = localtime(&timer);
	strftime(buf, len, format, tm_info);
}

void print_bits(s32 const size, void const *const ptr) {
	unsigned char *b = (unsigned char *)ptr;
	unsigned char  byte;
	s32            i, j;

	for (i = size - 1; i >= 0; i--) {
		for (j = 7; j >= 0; j--) {
			byte = (b[i] >> j) & 1;
			printf("%u", byte);
		}
	}
	puts("");
}

int count_bits(u64 n) {
	int count = 0;
	while (n) {
		count++;
		n = n >> 1;
	}
	return count;
}

str_t _get_dir_from_filepath(const char *ptr, const s32 len) {
	for (s32 i = len - 1; i-- > 0;) {
		if (ptr[i] == PATH_SEPARATORC) {
			return make_str(ptr, i);
		}
	}
	return str_empty;
}

str_t _get_filename_from_filepath(const char *ptr, const s32 len) {
	for (s32 i = len - 1; i-- > 0;) {
		if (ptr[i] == PATH_SEPARATORC) {
			return make_str(&ptr[i + 1], len - i - 1);
		}
	}
	return str_empty;
}

str_buf_t platform_lib_name(const str_t name) {
	str_buf_t tmp = get_tmp_str();
	if (!name.len) return tmp;

#if BL_PLATFORM_MACOS
	str_buf_append_fmt(&tmp, "lib{str}.dylib", name);
#elif BL_PLATFORM_LINUX
	str_buf_append_fmt(&tmp, "lib{str}.so", name);
#elif BL_PLATFORM_WIN
	str_buf_append_fmt(&tmp, "{str}.dll", name);
#else
	babort("Unknown dynamic library format.");
#endif
	return tmp;
}

f64 get_tick_ms(void) {
#if BL_PLATFORM_MACOS
	struct mach_timebase_info convfact;
	if (mach_timebase_info(&convfact) != 0) {
		uint64_t tick = mach_absolute_time();
		return (f64)((tick * convfact.numer) / (convfact.denom * 1000000));
	} else {
		struct timeval tv;
		gettimeofday(&tv, NULL);
		const f64 s = (f64)tv.tv_sec;
		const f64 u = (f64)tv.tv_usec;
		return (s * 1000.0) + (u / 1000.0);
	}
#elif BL_PLATFORM_LINUX
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (f64)ts.tv_sec * 1000. + (f64)ts.tv_nsec / 1000000.;
#elif BL_PLATFORM_WIN
	LARGE_INTEGER f;
	LARGE_INTEGER t;

	if (!QueryPerformanceFrequency(&f)) return 0.;
	if (!QueryPerformanceCounter(&t)) return 0.;

	return ((f64)t.QuadPart / (f64)f.QuadPart) * 1000.;
#else
	babort("Unknown dynamic library format.");
#endif
}

s32 get_last_error(char *buf, s32 buf_len) {
#if BL_PLATFORM_MACOS
	const s32 error_code = errno;
	if (!error_code) return 0;
	const char *msg = strerror(error_code);
	strncpy(buf, msg, buf_len);
	return strlen(msg);
#elif BL_PLATFORM_LINUX
	const s32 error_code = errno;
	if (!error_code) return 0;
	const char *msg = strerror(error_code);
	strncpy(buf, msg, buf_len);
	return strlen(msg);
#elif BL_PLATFORM_WIN
	const s32 error_code = GetLastError();
	if (error_code == 0) return 0;
	const DWORD msg_len = FormatMessageA(
	    FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK,
	    NULL,
	    error_code,
	    0,
	    buf,
	    buf_len,
	    NULL);
	return msg_len;
#else
	babort("Cannot get last error!");
#endif
}

u32 next_pow_2(u32 n) {
	u32 p = 1;
	if (n && !(n & (n - 1))) return n;
	while (p < n)
		p <<= 1;
	return p;
}

void color_print(FILE *stream, s32 color, const char *format, ...) {
	if (builder.is_initialized && builder.options->no_color) color = BL_NO_COLOR;
	va_list args;
	va_start(args, format);

#if BL_PLATFORM_WIN
	if (builder.options->legacy_colors) {
		s32 c;
		switch (color) {
		case BL_YELLOW:
			c = (0xE % 0x0F);
			break;
		case BL_RED:
			c = (0xC % 0x0F);
			break;
		case BL_BLUE:
			c = (0x9 % 0x0F);
			break;
		case BL_GREEN:
			c = (0xA % 0x0F);
			break;
		case BL_CYAN:
			c = (0xB % 0x0F);
			break;

		default:
			c = 0;
		}

		if (color != BL_NO_COLOR) {
			HANDLE handle =
			    stream == stderr ? GetStdHandle(STD_ERROR_HANDLE) : GetStdHandle(STD_OUTPUT_HANDLE);
			CONSOLE_SCREEN_BUFFER_INFO console_info;
			GetConsoleScreenBufferInfo(handle, &console_info);
			WORD saved_attributes = console_info.wAttributes;

			SetConsoleTextAttribute(handle, c);
			vfprintf(stream, format, args);
			SetConsoleTextAttribute(handle, saved_attributes);
		} else {
			vfprintf(stream, format, args);
		}
		va_end(args);
		return;
	}
#endif

	char *c;
	switch (color) {
	case BL_RED:
		c = "\x1b[31m";
		break;
	case BL_GREEN:
		c = "\x1b[32m";
		break;
	case BL_YELLOW:
		c = "\x1b[33m";
		break;
	case BL_BLUE:
		c = "\x1b[34m";
		break;
	case BL_CYAN:
		c = "\x1b[36m";
		break;
	default:
		c = "";
	}

	if (color != BL_NO_COLOR) fprintf(stream, "%s", c);
	vfprintf(stream, format, args);
	if (color != BL_NO_COLOR) fprintf(stream, "\x1b[0m");
	va_end(args);
}

s32 cpu_thread_count(void) {
#if BL_PLATFORM_WIN
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	return sysinfo.dwNumberOfProcessors;
#else
	s32 nprocs = sysconf(_SC_NPROCESSORS_ONLN);
	if (nprocs < 1) {
		return 8; // ehm
	}
	return nprocs;
#endif
}

str_buf_t execute(const char *cmd) {
	str_buf_t tmp  = get_tmp_str();
	FILE     *pipe = popen(cmd, "r");
	if (!pipe) {
		builder_error("Command '%s' failed!", cmd);
		return tmp;
	}
	char buffer[128];
	while (fgets(buffer, static_arrlenu(buffer), pipe) != NULL) {
		str_buf_append(&tmp, make_str_from_c(buffer));
	}
	pclose(pipe);
	if (tmp.len > 0 && tmp.ptr[tmp.len - 1] == '\n') {
		tmp.len -= 1;
		tmp.ptr[tmp.len] = '\0';
	}
	return tmp;
}

const char *read_config(struct config       *config,
                        const struct target *target,
                        const char          *path,
                        const char          *default_value) {
	bassert(config && target && path);
	str_buf_t fullpath = get_tmp_str();
	char      triple_str[TRIPLE_MAX_LEN];
	target_triple_to_string(&target->triple, triple_str, static_arrlenu(triple_str));
	str_buf_append_fmt(&fullpath, "/{s}/{s}", triple_str, path);
	const char *result = confreads(config, str_buf_to_c(fullpath), default_value);
	put_tmp_str(fullpath);
	return result;
}

s32 process_tokens(void *ctx, const char *input, const char *delimiter, process_tokens_fn_t fn) {
	if (!is_str_valid_nonempty(input)) return 0;
	str_buf_t tmp = get_tmp_str();
	str_buf_append(&tmp, make_str_from_c(input));
	char *token;
	char *it    = tmp.ptr;
	s32   count = 0;
	while ((token = strtok_r(it, delimiter, &it))) {
		token = strtrim(token);
		if (token[0] != '\0') {
			fn(ctx, token);
			++count;
		}
	}
	put_tmp_str(tmp);
	return count;
}

static char *ltrim(char *s) {
	while (isspace(*s))
		s++;
	return s;
}

static char *rtrim(char *s) {
	char *back = s + strlen(s);
	while (isspace(*--back))
		;
	*(back + 1) = '\0';
	return s;
}

char *strtrim(char *str) {
	return rtrim(ltrim(str));
}

char *str_dup_if_not_terminated(str_buf_t *tmp, char *ptr, const s32 len) {
	if (!ptr) {
		bassert(!len);
		return NULL;
	}
	if (ptr[len] == '\0') return ptr;
	str_buf_clr(tmp);
	str_buf_append(tmp, make_str(ptr, len));
	return tmp->ptr;
}
