// =================================================================================================
// bl
//
// File:   common.h
// Author: Martin Dorazil
// Date:   03/03/2018
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
// =================================================================================================

#ifndef BL_COMMON_H
#define BL_COMMON_H

// clang-format off
#include "blmemory.h"
// clang-format on
#include "TracyC.h"
#include "basic_types.h"
#include "bldebug.h"
#include "config.h"
#include "error.h"
#include "math.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>
#include <time.h>

struct assembly;
struct unit;
struct scope;
struct target;
struct config;

#if BL_PLATFORM_WIN
#include <Shlwapi.h>
#define PATH_MAX MAX_PATH
#ifndef strtok_r
#define strtok_r strtok_s
#endif
#endif

#if BL_COMPILER_CLANG || BL_COMPILER_GNUC

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

#if BL_PLATFORM_WIN
#define OBJ_EXT "obj"
#else
#define OBJ_EXT "o"
#endif

#define alignment_of(T) _Alignof(T)

#define isflag(_v, _flag) ((bool)(((_v) & (_flag)) == (_flag)))
#define isnotflag(_v, _flag) ((bool)(((_v) & (_flag)) != (_flag)))
#define setflag(_v, _flag) ((_v) |= (_flag))
#define clrflag(_v, _flag) ((_v) &= ~(_flag))
#define setflagif(_v, _flag, _toggle) ((_toggle) ? setflag(_v, _flag) : clrflag(_v, _flag))

enum { BL_RED,
	   BL_BLUE,
	   BL_YELLOW,
	   BL_GREEN,
	   BL_CYAN,
	   BL_NO_COLOR = -1 };

#define runtime_measure_begin(name) f64 __##name = get_tick_ms()
#define runtime_measure_end(name) ((s32)(get_tick_ms() - __##name))

#define LIB_NAME_MAX 256

// Return size of of static array.
#define static_arrlenu(A) (sizeof(A) / sizeof((A)[0]))
// @Cleanup?
#define is_str_valid_nonempty(S) ((S) && (S)[0] != '\0')

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef CLAMP
#define CLAMP(v, min, max) ((v) < (min) ? (v) = (min) : ((v) > (max) ? (v) = (max) : (v)))
#endif

// =================================================================================================
// String View
// =================================================================================================

#define make_str(p, l)                      \
	(str_t) {                               \
		.ptr = (char *)(p), .len = (s32)(l) \
	}

#define cstr(P)                                             \
	(str_t) {                                               \
		.ptr = (P), .len = (sizeof(P) / sizeof((P)[0])) - 1 \
	}

#define make_str_from_c(p)                                \
	(str_t) {                                             \
		.ptr = (char *)(p), .len = (s32)strlen((char *)p) \
	}

#define str_empty \
	(str_t) {     \
		0         \
	}

// Non-owning string representation with cached size. Note that the string might not be zero
// terminated. This way we can avoid calling strlen() every time and ew can reduce amount of string
// copying and allocations (e.g. identificators can point directly to the loaded file data).
typedef struct {
	s32   len;
	s32   _; // Gap to make this ABI compatible with BL strings. Note that we also use this gap in str_buf.
	char *ptr;
} str_t;

static_assert(sizeof(str_t) == 16, "Invalid size of string view type.");

bool str_match(str_t a, str_t b);

// =================================================================================================
// String Buffer
// =================================================================================================

str_t str_toupper(str_t str);
s32   levenshtein(const str_t s1, const str_t s2);

// String dynamic array buffer.
//
// Note it's guaranteed to be zero terminated, however use 'str_to_c' macro for safety!
// Keep layout ABI compatible with str_t!
struct str_buf {
	s32   len;
	s32   cap;
	char *ptr;
};

typedef struct str_buf str_buf_t;

void str_buf_free(str_buf_t *buf);

// Set the capacity (preallocates) space for 'cap' characters. Does nothing in case the 'cap' is
// smaller than zero. The preallocation includes extra space for zero terminator.
void str_buf_setcap(str_buf_t *buf, s32 cap);

void      _str_buf_append(str_buf_t *buf, char *ptr, s32 len);
str_buf_t _str_buf_dup(char *ptr, s32 len);

// Append our custom string buffer with formatted input.
// Formatting:
//   {s}   - Zero terminated C string.
//   {str} - Our string buffer or string view.
//   {s32} - Signed 32bit integer.
//   {u32} - Unsigned 32bit integer.
//   {s64} - Signed 64bit integer.
//   {u64} - Unsigned 64bit integer.
//
// The formatter implementation can be found in 'bvsnprint' feel free to implement missing formatting you need.
void str_buf_append_fmt(str_buf_t *buf, const char *fmt, ...);

void str_buf_clr(str_buf_t *buf); // This is also setting the zero

static inline const char *_str_to_c_checked(char *ptr, s32 len) {
	if (!len) return "";
	bassert(ptr);
	bassert(ptr[len] == '\0' && "String is not zero terminated!");
	return ptr;
}

// Converts the input string or str_t or str_buf_t to the C string. Zero termination is checked by
// assert.
// In case the buffer is not allocated, returns pointer to the static empty C string.
#define str_to_c(B) _Generic((B),                   \
	str_buf_t: _str_to_c_checked((B).ptr, (B).len), \
	str_t: _str_to_c_checked((B).ptr, (B).len))

// This way we can append another string buffer or view without any casting.
#define str_buf_append(B, S) _Generic((S),           \
	str_buf_t: _str_buf_append(B, (S).ptr, (S).len), \
	str_t: _str_buf_append(B, (S).ptr, (S).len))

#define str_buf_append_char(B, C) _str_buf_append(B, &(C), 1)

#define str_buf_dup(S) _Generic((S),           \
	str_buf_t: _str_buf_dup((S).ptr, (S).len), \
	str_t: _str_buf_dup((S).ptr, (S).len))

#define str_buf_view(B)                \
	(str_t) {                          \
		.len = (B).len, .ptr = (B).ptr \
	}

s32 bsnprint(char *buf, s32 buf_len, const char *fmt, ...);
s32 bvsnprint(char *buf, s32 buf_len, const char *fmt, va_list args);

// =================================================================================================
// STB utils
// =================================================================================================

// These are used for explicitness.
#define array(T) T *
#define hash_table(T) T *

#define queue_t(T) \
	struct {       \
		T  *q[2];  \
		s64 i;     \
		s32 qi;    \
	}

#define _qcurrent(Q) ((Q)->q[(Q)->qi])
#define _qother(Q) ((Q)->q[(Q)->qi ^ 1])
#define qmaybeswap(Q)                                                                       \
	((Q)->i >= arrlen(_qcurrent(Q))                                                         \
	     ? (arrsetlen(_qcurrent(Q), 0), (Q)->qi ^= 1, (Q)->i = 0, arrlen(_qcurrent(Q)) > 0) \
	     : (true))
#define qfree(Q) (arrfree((Q)->q[0]), arrfree((Q)->q[1]))
#define qpush_back(Q, V) arrput(_qother(Q), (V))
#define qpop_front(Q) (_qcurrent(Q)[(Q)->i++])
#define qsetcap(Q, c) (arrsetcap(_qother(Q), c))

// =================================================================================================
// Small Array
// =================================================================================================
#define sarr_t(T, C)    \
	struct {            \
		u32 len, cap;   \
		union {         \
			T *_data;   \
			T  _buf[C]; \
		};              \
	}

typedef sarr_t(u8, 1) sarr_any_t;

#define SARR_ZERO {.len = 0, .cap = 0}

#define sarradd(A) sarraddn(A, 1)
#define sarraddn(A, N)                                                                     \
	(sarradd_impl((A), sizeof((A)->_buf[0]), sizeof((A)->_buf) / sizeof((A)->_buf[0]), N), \
	 &sarrpeek(A, sarrlenu(A) - N))
#define sarrput(A, V) (*sarradd(A) = (V))
#define sarrpop(A) (sarrlenu(A) > 0 ? sarrpeek(A, --(A)->len) : (abort(), (A)->_data[0]))
#define sarrlenu(A) ((usize)((A) ? (A)->len : 0))
#define sarrlen(A) ((s64)((A) ? (A)->len : 0))
#define sarrdata(A) ((A)->cap ? ((A)->_data) : ((A)->_buf))
#define sarrpeek(A, I) (sarrdata(A)[I])
#define sarrpeekor(A, I, D) ((I) < sarrlenu(A) ? sarrdata(A)[I] : (D))
#define sarrclear(A) ((A)->len = 0)
#define sarrfree(A) ((A)->cap ? bfree((A)->_data) : (void)0, (A)->len = 0, (A)->cap = 0)
#define sarrsetlen(A, L)                       \
	{                                          \
		const s64 d = ((s64)(L)) - sarrlen(A); \
		if (d) sarraddn(A, d);                 \
	}                                          \
	(void)0

void sarradd_impl(void *ptr, usize elem_size, usize static_elem_count, usize new_elem_count);

typedef sarr_t(struct ast *, 16) ast_nodes_t;
typedef sarr_t(struct mir_arg *, 16) mir_args_t;
typedef sarr_t(struct mir_fn *, 16) mir_fns_t;
typedef sarr_t(struct mir_type *, 16) mir_types_t;
typedef sarr_t(struct mir_member *, 16) mir_members_t;
typedef sarr_t(struct mir_variant *, 16) mir_variants_t;
typedef sarr_t(struct mir_instr *, 16) mir_instrs_t;
typedef sarr_t(s64, 16) ints_t;

// =================================================================================================
// String cache
// =================================================================================================
struct string_cache;

// Allocate string inside the sting cache, a passed cache pointer must be initialized to NULL for
// the first time. The malloc is called only in case there is not enough space left for the string
// inside the preallocated block. Internally, len+1 is allocated to hold zero terminators. When
// 'str' is NULL no data copy is done. Function returns pointer to new allocated block/copy of the
// original string.
char *scdup(struct string_cache **cache, const char *str, usize len);

#define scdup2(C, S) _scdup2(C, (S).ptr, (S).len)
str_t _scdup2(struct string_cache **cache, char *str, s32 len);

void  scfree(struct string_cache **cache);
str_t scprint(struct string_cache **cache, const char *fmt, ...);

// =================================================================================================
// Hashing
// =================================================================================================
typedef u32 hash_t;

struct id {
	str_t  str;
	hash_t hash;
};

// Reference implementation: https://github.com/haipome/fnv/blob/master/fnv.c
#define strhash(S) _strhash((S).ptr, (S).len)
static inline hash_t _strhash(char *ptr, s32 len) {
	// FNV 32-bit hash
	hash_t hash = 2166136261;
	char   c;
	for (s32 i = 0; i < len; ++i) {
		c    = ptr[i];
		hash = hash ^ (u8)c;
		hash = hash * 16777619;
	}
	return hash;
}

static inline hash_t hashcomb(hash_t first, hash_t second) {
	return first ^ (second + 0x9e3779b9 + (first << 6) + (first >> 2));
}

static inline struct id *id_init(struct id *id, str_t str) {
	bassert(id);
	id->hash = strhash(str);
	id->str  = str;
	return id;
}

static inline bool is_ignored_id(struct id *id) {
	bassert(id);
	if (id->str.len != 1) return false;
	return id->str.ptr[0] == '_';
}

// =================================================================================================
// Utils
// =================================================================================================

typedef void (*unit_stage_fn_t)(struct assembly *, struct unit *);
typedef void (*assembly_stage_fn_t)(struct assembly *);

enum search_flags {
	// Just try to resolve normalized absolute path of the passed file.
	SEARCH_FLAG_ABS = 1 << 0,
	// Search in specified working directory.
	SEARCH_FLAG_WDIR = 1 << 1,
	// Search in compiler lib directory.
	SEARCH_FLAG_LIB_DIR = 1 << 2,
	// Search in system path.
	SEARCH_FLAG_SYSTEM_PATH = 1 << 3,

	SEARCH_FLAG_ALL = SEARCH_FLAG_WDIR | SEARCH_FLAG_LIB_DIR | SEARCH_FLAG_SYSTEM_PATH,
};

// Search file specified by 'filepath' and sets output filepath (full file location).
//
// Search order:
//     1) Current working directory.
//     2) wdir directory if not NULL.
//     3) LIB_DIR specified in global config file.
//     4) System PATH.
//
// Function returns true and modify output filepath if file was found otherwise returns false.
bool search_source_file(const str_t filepath, const u32 flags, const str_t wdir, str_buf_t *out_filepath);

static inline bool is_aligned(const void *p, usize alignment) {
	return (uintptr_t)p % alignment == 0;
}

#define is_aligned2(ptr, a) is_aligned((void *)(usize)(ptr), (a))

static inline void align_ptr_up(void **p, usize alignment, ptrdiff_t *adjustment) {
	ptrdiff_t adj;
	if (is_aligned(*p, alignment)) {
		if (adjustment) *adjustment = 0;
		return;
	}

	const usize mask = alignment - 1;
	bassert((alignment & mask) == 0 && "Wrong alignment!"); // pwr of 2
	const uintptr_t i_unaligned  = (uintptr_t)(*p);
	const uintptr_t misalignment = i_unaligned & mask;

	adj = alignment - misalignment;
	*p  = (void *)(i_unaligned + adj);
	if (adjustment) *adjustment = adj;
}

static inline void *next_aligned(void *p, usize alignment) {
	align_ptr_up(&p, alignment, NULL);
	return p;
}

#define next_aligned2(ptr, a) (usize) next_aligned((void *)(usize)(ptr), (a))

// Replace all backslashes in passed path with forward slash, this is used as workaround on Windows
// platform due to inconsistency 'Unix vs Windows' path separators. This function will modify passed
// buffer.
void        win_path_to_unix(str_t path);
void        unix_path_to_win(str_t path);
bool        file_exists(const str_t filepath);
bool        dir_exists(const str_t dirpath);
bool        normalize_path(str_buf_t *path);
bool        brealpath(const str_t path, str_buf_t *out_full_path);
bool        set_current_working_dir(const char *path);
str_t       get_dir_from_filepath(const str_t filepath);
str_t       get_filename_from_filepath(const str_t filepath);
bool        get_current_exec_path(str_buf_t *out_full_path);
bool        get_current_exec_dir(str_buf_t *out_full_path);
bool        create_dir(const str_t dirpath);
bool        create_dir_tree(const str_t dirpath);
bool        copy_dir(const str_t src, const str_t dest);
bool        copy_file(const str_t src, const str_t dest);
bool        remove_dir(const str_t path);
void        date_time(char *buf, s32 len, const char *format);
void        print_bits(s32 const size, void const *const ptr);
int         count_bits(u64 n);
str_buf_t   platform_lib_name(const str_t name);
f64         get_tick_ms(void);
s32         get_last_error(char *buf, s32 buf_len);
u32         next_pow_2(u32 n);
void        color_print(FILE *stream, s32 color, const char *format, ...);
s32         cpu_thread_count(void);
str_buf_t   execute(const char *cmd);
const char *read_config(struct config       *config,
                        const struct target *target,
                        const char          *path,
                        const char          *default_value);

typedef void (*process_tokens_fn_t)(void *ctx, const char *token);
s32   process_tokens(void *ctx, const char *input, const char *delimiter, process_tokens_fn_t fn);
char *strtrim(char *str);

#endif
