#include "common.h"
#include "atomics.h"
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

static int bl_ordering_to_c11(s32 ordering) {
	switch (ordering) {
	case 1:
		return BL_MEMORY_ORDER_RELAXED;
	case 2:
		return BL_MEMORY_ORDER_RELAXED;
	case 4:
		return BL_MEMORY_ORDER_ACQUIRE;
	case 5:
		return BL_MEMORY_ORDER_RELEASE;
	case 6:
		return BL_MEMORY_ORDER_ACQ_REL;
	case 7:
		return BL_MEMORY_ORDER_SEQ_CST;
	default:
		return BL_MEMORY_ORDER_SEQ_CST;
	}
}

// RMW
BL_EXPORT s64 __intrinsic_atomic_rmw_i64(s64 *ptr, s64 v, s32 ordering, s32 op) {
	return batomic_exchange_i64((batomic_s64 *)ptr, v, bl_ordering_to_c11(ordering), op);
}

BL_EXPORT s32 __intrinsic_atomic_rmw_i32(s32 *ptr, s32 v, s32 ordering, s32 op) {
	return batomic_exchange_i32((batomic_s32 *)ptr, v, bl_ordering_to_c11(ordering), op);
}

BL_EXPORT s16 __intrinsic_atomic_rmw_i16(s16 *ptr, s16 v, s32 ordering, s32 op) {
	return batomic_exchange_i16((batomic_s16 *)ptr, v, bl_ordering_to_c11(ordering), op);
}

BL_EXPORT s8 __intrinsic_atomic_rmw_i8(s8 *ptr, s8 v, s32 ordering, s32 op) {
	return batomic_exchange_i8((batomic_s8 *)ptr, v, bl_ordering_to_c11(ordering), op);
}

// Load
BL_EXPORT s64 __intrinsic_atomic_load_i64(s64 *ptr, s64 v, s32 ordering) {
	return batomic_load_i64((batomic_s64 *)ptr, bl_ordering_to_c11(ordering));
}

BL_EXPORT s32 __intrinsic_atomic_load_i32(s32 *ptr, s32 v, s32 ordering) {
	return batomic_load_i32((batomic_s32 *)ptr, bl_ordering_to_c11(ordering));
}

BL_EXPORT s16 __intrinsic_atomic_load_i16(s16 *ptr, s16 v, s32 ordering) {
	return batomic_load_i16((batomic_s16 *)ptr, bl_ordering_to_c11(ordering));
}

BL_EXPORT s8 __intrinsic_atomic_load_i8(s8 *ptr, s8 v, s32 ordering) {
	return batomic_load_i8((batomic_s8 *)ptr, bl_ordering_to_c11(ordering));
}

// Store
BL_EXPORT void __intrinsic_atomic_store_i64(s64 *ptr, s64 v, s32 ordering) {
	batomic_store_i64((batomic_s64 *)ptr, v, bl_ordering_to_c11(ordering));
}

BL_EXPORT void __intrinsic_atomic_store_i32(s32 *ptr, s32 v, s32 ordering) {
	batomic_store_i32((batomic_s32 *)ptr, v, bl_ordering_to_c11(ordering));
}

BL_EXPORT void __intrinsic_atomic_store_i16(s16 *ptr, s16 v, s32 ordering) {
	batomic_store_i16((batomic_s16 *)ptr, v, bl_ordering_to_c11(ordering));
}

BL_EXPORT void __intrinsic_atomic_store_i8(s8 *ptr, s8 v, s32 ordering) {
	batomic_store_i8((batomic_s8 *)ptr, v, bl_ordering_to_c11(ordering));
}

// Cmpxchg
BL_EXPORT bool __intrinsic_atomic_cmpxchg_i64(s64 *ptr, s64 *expected, s64 desired, s32 success, s32 failure) {
	return batomic_cmpxchg_i64((batomic_s64 *)ptr, expected, desired, bl_ordering_to_c11(success), bl_ordering_to_c11(failure));
}

BL_EXPORT bool __intrinsic_atomic_cmpxchg_i32(s32 *ptr, s32 *expected, s32 desired, s32 success, s32 failure) {
	return batomic_cmpxchg_i32((batomic_s32 *)ptr, expected, desired, bl_ordering_to_c11(success), bl_ordering_to_c11(failure));
}

BL_EXPORT bool __intrinsic_atomic_cmpxchg_i16(s16 *ptr, s16 *expected, s16 desired, s32 success, s32 failure) {
	return batomic_cmpxchg_i16((batomic_s16 *)ptr, expected, desired, bl_ordering_to_c11(success), bl_ordering_to_c11(failure));
}

BL_EXPORT bool __intrinsic_atomic_cmpxchg_i8(s8 *ptr, s8 *expected, s8 desired, s32 success, s32 failure) {
	return batomic_cmpxchg_i8((batomic_s8 *)ptr, expected, desired, bl_ordering_to_c11(success), bl_ordering_to_c11(failure));
}
