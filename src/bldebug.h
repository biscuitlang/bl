#ifndef BL_DEBUG_H
#define BL_DEBUG_H

#include "basic_types.h"
#include "config.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#if BL_PLATFORM_LINUX
#include <signal.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if BL_COMPILER_GNUC || BL_COMPILER_CLANG
#ifndef __FILENAME__
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif
#elif BL_COMPILER_MSVC
#ifndef __FILENAME__
#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#endif
#else
#pragma message("WARNING: Cannot parse filename with this compiler")
#define __FILENAME__
#endif

#define BL_STATIC_ASSERT(c, e) _Static_assert((c), e)

typedef enum { LOG_ASSERT,
	           LOG_ABORT,
	           LOG_ABORT_ISSUE,
	           LOG_WARNING,
	           LOG_MSG } log_msg_kind_t;

void log_impl(log_msg_kind_t t, const char *file, s32 line, const char *msg, ...);
void print_trace_impl(void);

#if defined(BL_DEBUG)
#define print_trace() print_trace_impl()
#else
#define print_trace() (void)0
#endif

#if defined(BL_DEBUG) || BL_ASSERT_ENABLE
// =================================================================================================
#if BL_PLATFORM_WIN
#define BL_DEBUG_BREAK __debugbreak()
#elif BL_PLATFORM_MACOS
#define BL_DEBUG_BREAK __builtin_debugtrap()
#else
#define BL_DEBUG_BREAK raise(SIGTRAP)
#endif

#define bassert(e)                                        \
	if (!(e)) {                                           \
		log_impl(LOG_ASSERT, __FILENAME__, __LINE__, #e); \
		print_trace();                                    \
		BL_DEBUG_BREAK;                                   \
		abort();                                          \
	}                                                     \
	(void)0

#define bmagic_assert(O)                                                  \
	{                                                                     \
		bassert(O && "Invalid reference!");                               \
		bassert((O)->_magic == (void *)&(O)->_magic && "Invalid magic!"); \
	}                                                                     \
	(void)0
#define bmagic_member void *_magic;
#define bmagic_set(O) (O)->_magic = (void *)&(O)->_magic

#define bcalled_once_member(name) s32 _##name##_call_count;
#define bcalled_once_assert(obj, name) \
	bassert((obj)->_##name##_call_count++ == 0 && "Expected to be called only once!")

#define bcheck_main_thread() bassert(thrd_equal(MAIN_THREAD, thrd_current()) && "Function is supposed to be called from the main thread!")
#define bcheck_true(expr)    bassert(expr)

// =================================================================================================
#else
// =================================================================================================
#define BL_DEBUG_BREAK \
	while (0) {        \
	}

#define bassert(e) \
	while (0) {    \
	}              \
	(void)0

#define bmagic_assert(O) \
	while (0) {          \
	}                    \
	(void)0

#define bmagic_member
#define bmagic_set(O) \
	while (0) {       \
	}                 \
	(void)0

#define bcalled_once_member(name)
#define bcalled_once_assert(obj, name) (void)0
#define bcheck_main_thread()           (void)0
#define bcheck_true(expr)              (expr)

// =================================================================================================
#endif

#if defined(BL_DEBUG)
// =================================================================================================
#define blog(format, ...)                                                 \
	{                                                                     \
		log_impl(LOG_MSG, __FILENAME__, __LINE__, format, ##__VA_ARGS__); \
	}                                                                     \
	(void)0

#define bwarn(format, ...)                                                    \
	{                                                                         \
		log_impl(LOG_WARNING, __FILENAME__, __LINE__, format, ##__VA_ARGS__); \
	}                                                                         \
	(void)0
// =================================================================================================
#else
// =================================================================================================
#define blog(format, ...) \
	while (0) {           \
	}                     \
	(void)0

#define bwarn(format, ...) \
	while (0) {            \
	}                      \
	(void)0
// =================================================================================================
#endif

#define babort(format, ...)                                                 \
	{                                                                       \
		log_impl(LOG_ABORT, __FILENAME__, __LINE__, format, ##__VA_ARGS__); \
		print_trace();                                                      \
		BL_DEBUG_BREAK;                                                     \
		abort();                                                            \
	}                                                                       \
	(void)0

#define babort_issue(N)                                                \
	{                                                                  \
		log_impl(LOG_ABORT_ISSUE,                                      \
		         __FILENAME__,                                         \
		         __LINE__,                                             \
		         "Issue: https://github.com/biscuitlang/bl/issues/%d", \
		         N);                                                   \
		print_trace();                                                 \
		BL_DEBUG_BREAK;                                                \
		abort();                                                       \
	}                                                                  \
	(void)0

#define BL_UNIMPLEMENTED                                              \
	{                                                                 \
		log_impl(LOG_ABORT, __FILENAME__, __LINE__, "unimplemented"); \
		print_trace();                                                \
		BL_DEBUG_BREAK;                                               \
		abort();                                                      \
	}                                                                 \
	(void)0

#define BL_UNREACHABLE                                              \
	{                                                               \
		log_impl(LOG_ABORT, __FILENAME__, __LINE__, "unreachable"); \
		print_trace();                                              \
		BL_DEBUG_BREAK;                                             \
		abort();                                                    \
	}                                                               \
	(void)0

#ifdef TRACY_ENABLE
#define BL_TRACY_MESSAGE(tag, format, ...)                                     \
	{                                                                          \
		char buf[256];                                                         \
		snprintf(buf, static_arrlenu(buf), "#%s " format, tag, ##__VA_ARGS__); \
		TracyCMessageC(buf, strlen(buf), strhash(make_str_from_c(tag)));       \
	}                                                                          \
	(void)0
#else
#define BL_TRACY_MESSAGE(tag, format, ...) \
	while (0) {                            \
	}                                      \
	(void)0
#endif

#define zone()               \
	TracyCZone(_tctx, true); \
	(void)0

#define _BL_VARGS(...) __VA_ARGS__
#define return_zone(...)                                    \
	{                                                       \
		TracyCZoneEnd(_tctx) return _BL_VARGS(__VA_ARGS__); \
	}                                                       \
	(void)0

#ifdef __cplusplus
}
#endif
#endif
