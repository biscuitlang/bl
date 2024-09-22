// =================================================================================================
// bl
//
// File:   intrinsic.c
// Author: Martin Dorazil
// Date:   8/1/20
//
// Copyright 2020 Martin Dorazil
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

#include "common.h"
#include <math.h>

BL_EXPORT void __intrinsic_memmove_p0_p0_i64(u8 *dest, u8 *src, usize size, bool is_volatile) {
	memmove(dest, src, size);
}

BL_EXPORT f32 __intrinsic_sin_f32(f32 v) {
	return sinf(v);
}

BL_EXPORT f64 __intrinsic_sin_f64(f64 v) {
	return sin(v);
}

BL_EXPORT f32 __intrinsic_cos_f32(f32 v) {
	return cosf(v);
}

BL_EXPORT f64 __intrinsic_cos_f64(f64 v) {
	return cos(v);
}

BL_EXPORT f32 __intrinsic_round_f32(f32 v) {
	return roundf(v);
}

BL_EXPORT f64 __intrinsic_round_f64(f64 v) {
	return round(v);
}

BL_EXPORT f32 __intrinsic_floor_f32(f32 v) {
	return floorf(v);
}

BL_EXPORT f64 __intrinsic_floor_f64(f64 v) {
	return floor(v);
}

BL_EXPORT f32 __intrinsic_pow_f32(f32 v1, f32 v2) {
	return powf(v1, v2);
}

BL_EXPORT f64 __intrinsic_pow_f64(f64 v1, f64 v2) {
	return pow(v1, v2);
}

BL_EXPORT f32 __intrinsic_exp_f32(f32 v1) {
	return (f32)exp(v1);
}

BL_EXPORT f64 __intrinsic_exp_f64(f64 v1) {
	return exp(v1);
}

BL_EXPORT f32 __intrinsic_log_f32(f32 v) {
	return logf(v);
}

BL_EXPORT f64 __intrinsic_log_f64(f64 v) {
	return log(v);
}

BL_EXPORT f32 __intrinsic_log2_f32(f32 v) {
	return log2f(v);
}

BL_EXPORT f64 __intrinsic_log2_f64(f64 v) {
	return log2(v);
}

BL_EXPORT f32 __intrinsic_log10_f32(f32 v) {
	return log10f(v);
}

BL_EXPORT f64 __intrinsic_log10_f64(f64 v) {
	return log10(v);
}

BL_EXPORT f32 __intrinsic_sqrt_f32(f32 v) {
	return sqrtf(v);
}

BL_EXPORT f64 __intrinsic_sqrt_f64(f64 v) {
	return sqrt(v);
}

BL_EXPORT f32 __intrinsic_ceil_f32(f32 v) {
	return ceilf(v);
}

BL_EXPORT f64 __intrinsic_ceil_f64(f64 v) {
	return ceil(v);
}

BL_EXPORT f32 __intrinsic_trunc_f32(f32 v) {
	return truncf(v);
}

BL_EXPORT f64 __intrinsic_trunc_f64(f64 v) {
	return trunc(v);
}
