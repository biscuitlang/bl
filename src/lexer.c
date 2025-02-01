// =================================================================================================
// blc
//
// File:   lexer_new.c
// Author: Martin Dorazil
// Date:   19/02/2018
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
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILItTY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
// =================================================================================================

#include "builder.h"
#include "tokens_inline_utils.h"
#include <ctype.h>
#include <float.h>
#include <setjmp.h>
#include <string.h>

#ifdef BL_USE_SIMD
#include <emmintrin.h>
#include <intrin.h>
#endif

#define is_ident(c) (isalnum(c) || (c) == '_')

#define report_error(code, format, ...)                                              \
	{                                                                                \
		builder_msg(MSG_ERR, ERR_##code, NULL, CARET_NONE, (format), ##__VA_ARGS__); \
		longjmp((ctx)->jmp_error, ERR_##code);                                       \
	}                                                                                \
	(void)0

struct context {
	struct assembly      *assembly;
	struct unit          *unit;
	struct tokens        *tokens;
	struct string_cache **string_cache;
	sarr_t(char, 64) strtmp; // @Cleanup: Use tmp string from builder.
	char *c;
	s32   line;
	s32   col;

	jmp_buf jmp_error;
};

static void       scan(struct context *ctx);
static bool       scan_comment(struct context *ctx, struct token *tok, const char *term);
static bool       scan_docs(struct context *ctx, struct token *tok);
static bool       scan_ident(struct context *ctx, struct token *tok);
static bool       scan_string(struct context *ctx, struct token *tok);
static bool       scan_char(struct context *ctx, struct token *tok);
static bool       scan_number(struct context *ctx, struct token *tok);
static inline int c_to_number(char c, s32 base);
static char       scan_specch(char c);

static inline u32 add_token_value(struct context *ctx, union token_value value) {
	const u32 index = (u32)arrlenu(ctx->tokens->values);
	arrput(ctx->tokens->values, value);
	return index;
}

bool scan_comment(struct context *ctx, struct token *tok, const char *term) {
	if (tok->sym == SYM_SHEBANG && ctx->line != 1) {
		report_error(INVALID_TOKEN, STR_FMT " %d:%d Shebang is allowed only on the first line of the file.", STR_ARG(ctx->unit->name), ctx->line, ctx->col);
	}
	const usize len = strlen(term);
	while (true) {
		if (*ctx->c == '\n') {
			ctx->line++;
			ctx->col = 1;
		} else if (*ctx->c == '\0' && strcmp(term, "\n") != 0) {
			// Unterminated comment
			report_error(UNTERMINATED_COMMENT, STR_FMT " %d:%d unterminated comment block.", STR_ARG(ctx->unit->name), ctx->line, ctx->col);
		}
		if (*ctx->c == SYM_EOF) return true;
		if (strncmp(ctx->c, term, len) == 0) break;
		ctx->c++;
	}
	ctx->c += len;
	return true;
}

bool scan_docs(struct context *ctx, struct token *tok) {
	bassert(token_is(tok, SYM_DCOMMENT) || token_is(tok, SYM_DGCOMMENT));
	tok->location.line = ctx->line;
	tok->location.col  = ctx->col;

	sarrclear(&ctx->strtmp);
	char *begin      = ctx->c;
	s32   len_parsed = 0;
	s32   len_str    = 0;
	while (true) {
		if (*ctx->c == SYM_EOF) break;
		if (*ctx->c == '\r' || *ctx->c == '\n') break;
		if (!len_parsed && *ctx->c == ' ') {
			++begin;
		} else {
			len_str++;
		}
		len_parsed++;
		ctx->c++;
	}

	str_t str        = scdup2(ctx->string_cache, make_str(begin, len_str));
	tok->value_index = add_token_value(ctx, (union token_value){.str = str});

	tok->location.len = len_parsed + 3; // + 3 = '///'
	ctx->col += len_parsed;
	return true;
}

bool scan_ident(struct context *ctx, struct token *tok) {
	zone();
	tok->location.line = ctx->line;
	tok->location.col  = ctx->col;
	tok->sym           = SYM_IDENT;

	char *begin = ctx->c;

	s32 len = 0;

#ifdef BL_USE_SIMD
	while (true) {
		// Process by 16 chars
		const __m128i src = _mm_loadu_si128((__m128i *)(begin + len));

		// Convert to uppercase
		const __m128i range_shift = _mm_sub_epi8(src, _mm_set1_epi8((char)('a' + 128)));
		const __m128i no_modify   = _mm_cmpgt_epi8(range_shift, _mm_set1_epi8(-128 + 25));
		const __m128i flip        = _mm_andnot_si128(no_modify, _mm_set1_epi8(0x20));
		const __m128i src_upper   = _mm_xor_si128(src, flip);

		// Check for characters from A to Z.
		const __m128i a = _mm_set1_epi8('A' - 1);
		const __m128i z = _mm_set1_epi8('Z' + 1);

		const __m128i gta      = _mm_cmpgt_epi8(src_upper, a);
		const __m128i ltz      = _mm_cmplt_epi8(src_upper, z);
		__m128i       is_alpha = _mm_and_si128(gta, ltz);

		// Check for underscore.
		const __m128i underscore = _mm_cmpeq_epi8(src_upper, _mm_set1_epi8('_'));
		is_alpha                 = _mm_or_si128(is_alpha, underscore);

		// Check for numbers
		const __m128i zero = _mm_set1_epi8('0' - 1);
		const __m128i nine = _mm_set1_epi8('9' + 1);

		const __m128i gtzero    = _mm_cmpgt_epi8(src_upper, zero);
		const __m128i ltnine    = _mm_cmplt_epi8(src_upper, nine);
		const __m128i is_number = _mm_and_si128(gtzero, ltnine);

		// Finalize
		const __m128i is_ident = _mm_or_si128(is_alpha, is_number);

		const int mask = _mm_movemask_epi8(is_ident) ^ 0xFFFF;
		int       advance;
		if (mask) {
			advance = _tzcnt_u32(mask);
		} else {
			advance = 16;
		}

		len += advance;
		if (mask) break;
	}

	// Shift the global cursor.
	ctx->c += len;
#else
	while (true) {
		if (!is_ident(*ctx->c)) {
			break;
		}

		len++;
		ctx->c++;
	}
#endif

	if (len == 0) return_zone(false);
	// Note that we use the string identificators directly (no copy is done). That means those might
	// not to be zero terminated! This way we reduce amount of string duplication.
	str_t str         = make_str(begin, len);
	tok->value_index  = add_token_value(ctx, (union token_value){.str = str});
	tok->location.len = len;
	ctx->col += len;
	return_zone(true);
}

inline char scan_specch(char c) {
	switch (c) {
	case 'n':
		return '\n';
	case 'r':
		return '\r';
	case 't':
		return '\t';
	case '0':
		return '\0';
	case '\"':
		return '\"';
	case '\'':
		return '\'';
	case '\\':
		return '\\';
	default:
		return c;
	}
}

bool scan_string(struct context *ctx, struct token *tok) {
	if (*ctx->c != '"') return false;
	zone();
	sarrclear(&ctx->strtmp);
	tok->location.line = ctx->line;
	tok->location.col  = ctx->col;
	tok->sym           = SYM_STRING;
	char c;
	s32  len = 0;

	// eat "
	ctx->c++;
	while (true) {
		switch (*ctx->c) {
		case '"': {
			ctx->c++;
			goto DONE;
		}
		case '\0': {
			report_error(UNTERMINATED_STRING, STR_FMT " %d:%d unterminated string.", STR_ARG(ctx->unit->name), ctx->line, ctx->col);
		}
		case '\\':
			// special character
			c = scan_specch(*(ctx->c + 1));
			ctx->c += 2;
			len += 2;
			break;
		default:
			c = *ctx->c;
			len++;
			ctx->c++;
		}
		sarrput(&ctx->strtmp, c);
	}
DONE: {
	str_t str         = scdup2(ctx->string_cache, make_str(sarrdata(&ctx->strtmp), sarrlenu(&ctx->strtmp)));
	tok->value_index  = add_token_value(ctx, (union token_value){.str = str});
	tok->location.len = len;
	tok->location.col += 1;
	ctx->col += len + 2;
	return_zone(true);
}
}

bool scan_char(struct context *ctx, struct token *tok) {
	if (*ctx->c != '\'') return false;
	tok->location.line = ctx->line;
	tok->location.col  = ctx->col;
	tok->location.len  = 0;
	tok->sym           = SYM_CHAR;

	// eat '
	ctx->c++;

	switch (*ctx->c) {
	case '\'': {
		report_error(EMPTY, STR_FMT " %d:%d expected character in ''.", STR_ARG(ctx->unit->name), ctx->line, ctx->col);
	}
	case '\0': {
		report_error(UNTERMINATED_STRING, STR_FMT " %d:%d unterminated character.", STR_ARG(ctx->unit->name), ctx->line, ctx->col);
	}
	case '\\':
		// special character
		tok->value_index = add_token_value(ctx, (union token_value){.character = scan_specch(*(ctx->c + 1))});
		ctx->c += 2;
		tok->location.len = 2;
		break;
	default:
		tok->value_index  = add_token_value(ctx, (union token_value){.character = *ctx->c});
		tok->location.len = 1;
		ctx->c++;
	}

	// eat '
	if (*ctx->c != '\'') {
		report_error(UNTERMINATED_STRING, STR_FMT " %d:%d unterminated character expected '.", STR_ARG(ctx->unit->name), ctx->line, ctx->col);
	}
	ctx->c++;

	return true;
}

inline s32 c_to_number(char c, s32 base) {
#ifndef _MSC_VER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif
	switch (base) {
	case 16:
		if (c >= 'a' && c <= 'f') {
			return c - 'a' + 10;
		}

		if (c >= 'A' && c <= 'F') {
			return c - 'A' + 10;
		}
	case 10:
		if (c >= '2' && c <= '9') {
			return c - '0';
		}
	case 2:
		if (c == '0' || c == '1') {
			return c - '0';
		}
		break;
	default:
		babort("invalid number base");
	}

	return -1;
#ifndef _MSC_VER
#pragma GCC diagnostic pop
#endif
}

bool scan_number(struct context *ctx, struct token *tok) {
	tok->location.line = ctx->line;
	tok->location.col  = ctx->col;

	u64 n = 0, prev_n = 0;
	s32 len  = 0;
	s32 base = 10;
	s32 buf  = 0;

	if (strncmp(ctx->c, "0x", 2) == 0) {
		base = 16;
		ctx->c += 2;
		len += 2;
	} else if (strncmp(ctx->c, "0b", 2) == 0) {
		base = 2;
		ctx->c += 2;
		len += 2;
	}

	bool overflow = false;

	while (true) {
		if (*(ctx->c) == '.') {

			if (base != 10) {
				report_error(INVALID_TOKEN, STR_FMT " %d:%d invalid suffix.", STR_ARG(ctx->unit->name), ctx->line, ctx->col + len);
			}

			len++;
			ctx->c++;
			goto SCAN_DOUBLE;
		}

		buf = c_to_number(*(ctx->c), base);
		if (buf == -1) {
			break;
		}

		prev_n = n;
		n      = n * base + buf;

		len++;
		ctx->c++;
		if (n < prev_n) overflow = true;
	}

	if (len == 0) return false;

	tok->location.len = len;
	ctx->col += len;
	tok->sym         = SYM_NUM;
	tok->value_index = add_token_value(ctx, (union token_value){.number = n});

	if (overflow) {
		builder_msg(MSG_ERR, ERR_NUM_LIT_OVERFLOW, &tok->location, CARET_WORD, "Number value overflow.");
	}

	return true;

SCAN_DOUBLE: {
	u64 e = 1;

	while (true) {
		buf = c_to_number(*(ctx->c), 10);
		if (buf == -1) {
			break;
		}

		prev_n = n;
		n      = n * 10 + buf;
		e *= 10;
		len++;
		ctx->c++;

		if (n < prev_n) overflow = true;
	}

	// valid d. or .d -> minimal 2 characters
	if (len < 2) return false;

	if (*(ctx->c) == 'f') {
		len++;
		ctx->c++;
		tok->sym = SYM_FLOAT;
	} else {
		tok->sym = SYM_DOUBLE;
	}

	const double number = n / (f64)e;
	if (number > FLT_MAX) overflow = true;
	if (overflow) {
		builder_msg(MSG_ERR, ERR_NUM_LIT_OVERFLOW, &tok->location, CARET_WORD, "Number value overflow.");
	}
	tok->value_index = add_token_value(ctx, (union token_value){.double_number = number});

	tok->location.len = len;
	ctx->col += len;

	return true;
}
}

void scan(struct context *ctx) {
	struct token tok = {0};
SCAN:
	tok.location.line = ctx->line;
	tok.location.col  = ctx->col;

	// Ignored characters
	switch (*ctx->c) {
	case '\0':
		tok.sym = SYM_EOF;
		tokens_push(ctx->tokens, tok);
		return;
	case '\r':
		ctx->c++;
		goto SCAN;
	case '\n':
		ctx->line++;
		ctx->col = 1;
		ctx->c++;
		goto SCAN;
	case '\t':
		ctx->col += 1;
		ctx->c++;
		goto SCAN;
	case ' ':
		ctx->col++;
		ctx->c++;
		goto SCAN;
	default:
		break;
	}

	// Scan symbols described directly as strings.
	usize      len                   = 0;
	const bool is_current_char_ident = is_ident(*ctx->c);
	const s32  start                 = is_current_char_ident ? SYM_IF : SYM_UNREACHABLE + 1;
	const s32  end                   = is_current_char_ident ? SYM_UNREACHABLE + 1 : SYM_NONE;
	for (s32 i = start; i < end; ++i) {
		len = sym_lens[i];
		bassert(len > 0);
		if (ctx->c[0] == sym_strings[i][0] && strncmp(ctx->c, sym_strings[i], len) == 0) {
			ctx->c += len;
			tok.sym          = (enum sym)i;
			tok.location.len = (s32)len;

			// Two joined symbols will be parsed as identifier.
			if (i >= SYM_IF && i <= SYM_UNREACHABLE && is_ident(*ctx->c)) {
				// roll back
				ctx->c -= len;
				break;
			}

#ifndef _MSC_VER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif
			switch (tok.sym) {
			case SYM_DCOMMENT:
			case SYM_DGCOMMENT:
				if (ctx->assembly && ctx->assembly->target->kind == ASSEMBLY_DOCS) {
					scan_docs(ctx, &tok);
					goto PUSH_TOKEN;
				}
			case SYM_SHEBANG:
			case SYM_LCOMMENT:
				scan_comment(ctx, &tok, "\n");
				goto SCAN;
			case SYM_LBCOMMENT:
				scan_comment(ctx, &tok, sym_strings[SYM_RBCOMMENT]);
				goto SCAN;
			case SYM_RBCOMMENT: {
				report_error(INVALID_TOKEN, STR_FMT " %d:%d unexpected token.", STR_ARG(ctx->unit->name), ctx->line, ctx->col);
			}
			default:
				ctx->col += (s32)len;
				goto PUSH_TOKEN;
			}
#ifndef _MSC_VER
#pragma GCC diagnostic pop
#endif
		}
	}

	// Scan special tokens.
	if (scan_number(ctx, &tok)) goto PUSH_TOKEN;
	if (scan_ident(ctx, &tok)) goto PUSH_TOKEN;
	if (scan_string(ctx, &tok)) goto PUSH_TOKEN;
	if (scan_char(ctx, &tok)) goto PUSH_TOKEN;

	// When symbol is unknown report error
	report_error(INVALID_TOKEN, STR_FMT " %d:%d Unexpected token '%c' (%d)", STR_ARG(ctx->unit->name), ctx->line, ctx->col, *ctx->c, *ctx->c);
PUSH_TOKEN:
	tok.location.unit = ctx->unit;
	tokens_push(ctx->tokens, tok);

	goto SCAN;
}

void lexer_run(struct assembly *assembly, struct unit *unit) {
	runtime_measure_begin(lex);

	const u32 thread_index = get_worker_index();

	struct context ctx = {
	    .assembly     = assembly,
	    .tokens       = &unit->tokens,
	    .unit         = unit,
	    .c            = unit->src,
	    .line         = 1,
	    .col          = 1,
	    .strtmp       = SARR_ZERO,
	    .string_cache = &assembly->thread_local_contexts[thread_index].string_cache,
	};

	zone();
	s32 error = 0;
	if ((error = setjmp(ctx.jmp_error))) {
		sarrfree(&ctx.strtmp);
		batomic_fetch_add_s32(&assembly->stats.lexing_ms, runtime_measure_end(lex));
		return_zone();
	}

	scan(&ctx);
	sarrfree(&ctx.strtmp);

	batomic_fetch_add_s32(&builder.total_lines, ctx.line);
	batomic_fetch_add_s32(&assembly->stats.lexing_ms, runtime_measure_end(lex));
	return_zone();
}
