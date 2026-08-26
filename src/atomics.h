#ifndef BL_ATOMICS_H
#define BL_ATOMICS_H

#include "common.h"
#include "basic_types.h"

enum bl_atomic_op {
	BL_ATOMIC_OP_XCHG = 0,
	BL_ATOMIC_OP_ADD  = 1,
	BL_ATOMIC_OP_SUB  = 2,
	BL_ATOMIC_OP_AND  = 3,
	BL_ATOMIC_OP_OR   = 4,
	BL_ATOMIC_OP_XOR  = 5,
};

#define batomic_load_s32(a)           batomic_load_i32((a), BL_MEMORY_ORDER_SEQ_CST)
#define batomic_load_s64(a)           batomic_load_i64((a), BL_MEMORY_ORDER_SEQ_CST)
#define batomic_store_s32(a, val)     batomic_store_i32((a), (val), BL_MEMORY_ORDER_SEQ_CST)
#define batomic_store_s64(a, val)     batomic_store_i64((a), (val), BL_MEMORY_ORDER_SEQ_CST)
#define batomic_fetch_add_s32(a, val) batomic_exchange_i32((a), (val), BL_MEMORY_ORDER_SEQ_CST, BL_ATOMIC_OP_ADD)
#define batomic_fetch_add_u32(a, val) batomic_exchange_i32((batomic_s32 *)(a), (val), BL_MEMORY_ORDER_SEQ_CST, BL_ATOMIC_OP_ADD)
#define batomic_fetch_add_s64(a, val) batomic_exchange_i64((a), (val), BL_MEMORY_ORDER_SEQ_CST, BL_ATOMIC_OP_ADD)

#if BL_PLATFORM_WIN

// Interlocked* functions always use sequential consistency ordering so this is only placeholder to keep api consistent.
enum bl_memory_order {
	BL_MEMORY_ORDER_RELAXED,
	BL_MEMORY_ORDER_CONSUME,
	BL_MEMORY_ORDER_ACQUIRE,
	BL_MEMORY_ORDER_RELEASE,
	BL_MEMORY_ORDER_ACQ_REL,
	BL_MEMORY_ORDER_SEQ_CST,
};

typedef volatile char          batomic_s8;
typedef volatile unsigned char batomic_u8;
typedef volatile SHORT         batomic_s16;
typedef volatile USHORT        batomic_u16;
typedef volatile LONG          batomic_s32;
typedef volatile ULONG         batomic_u32;
typedef volatile LONG64        batomic_s64;
typedef volatile ULONG64       batomic_u64;

static inline s8 batomic_load_i8(batomic_s8 *ptr, enum bl_memory_order ordering) {
	return _InterlockedCompareExchange8(ptr, 0, 0);
}

static inline s16 batomic_load_i16(batomic_s16 *ptr, enum bl_memory_order ordering) {
	return InterlockedCompareExchange16(ptr, 0, 0);
}

static inline s32 batomic_load_i32(batomic_s32 *ptr, enum bl_memory_order ordering) {
	return InterlockedCompareExchange(ptr, 0, 0);
}

static inline s64 batomic_load_i64(batomic_s64 *ptr, enum bl_memory_order ordering) {
	return InterlockedCompareExchange64(ptr, 0, 0);
}

static inline void batomic_store_i8(batomic_s8 *ptr, s8 val, enum bl_memory_order ordering) {
	InterlockedExchange8(ptr, val);
}

static inline void batomic_store_i16(batomic_s16 *ptr, s16 val, enum bl_memory_order ordering) {
	InterlockedExchange16(ptr, val);
}

static inline void batomic_store_i32(batomic_s32 *ptr, s32 val, enum bl_memory_order ordering) {
	InterlockedExchange(ptr, val);
}

static inline void batomic_store_i64(batomic_s64 *ptr, s64 val, enum bl_memory_order ordering) {
	InterlockedExchange64(ptr, val);
}

static inline s8 batomic_exchange_i8(batomic_s8 *ptr, s8 val, enum bl_memory_order ordering, enum bl_atomic_op op) {
	s8 old = batomic_load_i8(ptr, ordering);
	for (;;) {
		s8 desired;
		switch (op) {
		case BL_ATOMIC_OP_XCHG:
			desired = val;
			break;
		case BL_ATOMIC_OP_ADD:
			desired = old + val;
			break;
		case BL_ATOMIC_OP_SUB:
			desired = old - val;
			break;
		case BL_ATOMIC_OP_AND:
			desired = old & val;
			break;
		case BL_ATOMIC_OP_OR:
			desired = old | val;
			break;
		case BL_ATOMIC_OP_XOR:
			desired = old ^ val;
			break;
		default:
			return old;
		}
		s8 got = _InterlockedCompareExchange8(ptr, desired, old);
		if (got == old) return old;
		old = got;
	}
}

static inline s16 batomic_exchange_i16(batomic_s16 *ptr, s16 val, enum bl_memory_order ordering, enum bl_atomic_op op) {
	s16 old = batomic_load_i16(ptr, ordering);
	for (;;) {
		s16 desired;
		switch (op) {
		case BL_ATOMIC_OP_XCHG:
			desired = val;
			break;
		case BL_ATOMIC_OP_ADD:
			desired = old + val;
			break;
		case BL_ATOMIC_OP_SUB:
			desired = old - val;
			break;
		case BL_ATOMIC_OP_AND:
			desired = old & val;
			break;
		case BL_ATOMIC_OP_OR:
			desired = old | val;
			break;
		case BL_ATOMIC_OP_XOR:
			desired = old ^ val;
			break;
		default:
			return old;
		}
		s16 got = InterlockedCompareExchange16(ptr, desired, old);
		if (got == old) return old;
		old = got;
	}
}

static inline s32 batomic_exchange_i32(batomic_s32 *ptr, s32 val, enum bl_memory_order ordering, enum bl_atomic_op op) {
	switch (op) {
	case BL_ATOMIC_OP_XCHG:
		return InterlockedExchange(ptr, val);
	case BL_ATOMIC_OP_ADD: {
		s32 new_val = InterlockedAdd(ptr, val);
		return new_val - val;
	}
	case BL_ATOMIC_OP_SUB: {
		s32 new_val = InterlockedAdd(ptr, -val);
		return new_val + val;
	}
	default: {
		s32 old = batomic_load_i32(ptr, ordering);
		for (;;) {
			s32 desired;
			switch (op) {
			case BL_ATOMIC_OP_AND:
				desired = old & val;
				break;
			case BL_ATOMIC_OP_OR:
				desired = old | val;
				break;
			case BL_ATOMIC_OP_XOR:
				desired = old ^ val;
				break;
			default:
				return old;
			}
			s32 got = InterlockedCompareExchange(ptr, desired, old);
			if (got == old) return old;
			old = got;
		}
	}
	}
}

static inline s64 batomic_exchange_i64(batomic_s64 *ptr, s64 val, enum bl_memory_order ordering, enum bl_atomic_op op) {
	switch (op) {
	case BL_ATOMIC_OP_XCHG:
		return InterlockedExchange64(ptr, val);
	case BL_ATOMIC_OP_ADD: {
		s64 new_val = InterlockedAdd64(ptr, val);
		return new_val - val;
	}
	case BL_ATOMIC_OP_SUB: {
		s64 new_val = InterlockedAdd64(ptr, -val);
		return new_val + val;
	}
	default: {
		s64 old = batomic_load_i64(ptr, ordering);
		for (;;) {
			s64 desired;
			switch (op) {
			case BL_ATOMIC_OP_AND:
				desired = old & val;
				break;
			case BL_ATOMIC_OP_OR:
				desired = old | val;
				break;
			case BL_ATOMIC_OP_XOR:
				desired = old ^ val;
				break;
			default:
				return old;
			}
			s64 got = InterlockedCompareExchange64(ptr, desired, old);
			if (got == old) return old;
			old = got;
		}
	}
	}
}

static inline bool batomic_cmpxchg_i8(batomic_s8 *ptr, s8 *expected, s8 desired, enum bl_memory_order success, enum bl_memory_order failure) {
	s8 old = *expected;
	s8 got = _InterlockedCompareExchange8(ptr, desired, old);
	if (got == old) return true;
	*expected = got;
	return false;
}

static inline bool batomic_cmpxchg_i16(batomic_s16 *ptr, s16 *expected, s16 desired, enum bl_memory_order success, enum bl_memory_order failure) {
	s16 old = *expected;
	s16 got = _InterlockedCompareExchange16(ptr, desired, old);
	if (got == old) return true;
	*expected = got;
	return false;
}

static inline bool batomic_cmpxchg_i32(batomic_s32 *ptr, s32 *expected, s32 desired, enum bl_memory_order success, enum bl_memory_order failure) {
	s32 old = *expected;
	s32 got = InterlockedCompareExchange(ptr, desired, old);
	if (got == old) return true;
	*expected = got;
	return false;
}

static inline bool batomic_cmpxchg_i64(batomic_s64 *ptr, s64 *expected, s64 desired, enum bl_memory_order success, enum bl_memory_order failure) {
	s64 old = *expected;
	s64 got = InterlockedCompareExchange64(ptr, desired, old);
	if (got == old) return true;
	*expected = got;
	return false;
}

#else

#include <stdatomic.h>

enum bl_memory_order {
	BL_MEMORY_ORDER_RELAXED = memory_order_relaxed,
	BL_MEMORY_ORDER_CONSUME = memory_order_consume,
	BL_MEMORY_ORDER_ACQUIRE = memory_order_acquire,
	BL_MEMORY_ORDER_RELEASE = memory_order_release,
	BL_MEMORY_ORDER_ACQ_REL = memory_order_acq_rel,
	BL_MEMORY_ORDER_SEQ_CST = memory_order_seq_cst,
};

typedef _Atomic s8  batomic_s8;
typedef _Atomic u8  batomic_u8;
typedef _Atomic s16 batomic_s16;
typedef _Atomic u16 batomic_u16;
typedef _Atomic s32 batomic_s32;
typedef _Atomic u32 batomic_u32;
typedef _Atomic s64 batomic_s64;
typedef _Atomic u64 batomic_u64;

static inline s8 batomic_load_i8(batomic_s8 *ptr, enum bl_memory_order ordering) {
	return atomic_load_explicit(ptr, (memory_order)ordering);
}

static inline s16 batomic_load_i16(batomic_s16 *ptr, enum bl_memory_order ordering) {
	return atomic_load_explicit(ptr, (memory_order)ordering);
}

static inline s32 batomic_load_i32(batomic_s32 *ptr, enum bl_memory_order ordering) {
	return atomic_load_explicit(ptr, (memory_order)ordering);
}

static inline s64 batomic_load_i64(batomic_s64 *ptr, enum bl_memory_order ordering) {
	return atomic_load_explicit(ptr, (memory_order)ordering);
}

static inline void batomic_store_i8(batomic_s8 *ptr, s8 val, enum bl_memory_order ordering) {
	atomic_store_explicit(ptr, val, (memory_order)ordering);
}

static inline void batomic_store_i16(batomic_s16 *ptr, s16 val, enum bl_memory_order ordering) {
	atomic_store_explicit(ptr, val, (memory_order)ordering);
}

static inline void batomic_store_i32(batomic_s32 *ptr, s32 val, enum bl_memory_order ordering) {
	atomic_store_explicit(ptr, val, (memory_order)ordering);
}

static inline void batomic_store_i64(batomic_s64 *ptr, s64 val, enum bl_memory_order ordering) {
	atomic_store_explicit(ptr, val, (memory_order)ordering);
}

static inline s8 batomic_exchange_i8(batomic_s8 *ptr, s8 val, enum bl_memory_order ordering, enum bl_atomic_op op) {
	switch (op) {
	case BL_ATOMIC_OP_XCHG:
		return atomic_exchange_explicit(ptr, val, (memory_order)ordering);
	case BL_ATOMIC_OP_ADD:
		return atomic_fetch_add_explicit(ptr, val, (memory_order)ordering);
	case BL_ATOMIC_OP_SUB:
		return atomic_fetch_sub_explicit(ptr, val, (memory_order)ordering);
	case BL_ATOMIC_OP_AND:
		return atomic_fetch_and_explicit(ptr, val, (memory_order)ordering);
	case BL_ATOMIC_OP_OR:
		return atomic_fetch_or_explicit(ptr, val, (memory_order)ordering);
	case BL_ATOMIC_OP_XOR:
		return atomic_fetch_xor_explicit(ptr, val, (memory_order)ordering);
	default:
		return 0;
	}
}

static inline s16 batomic_exchange_i16(batomic_s16 *ptr, s16 val, enum bl_memory_order ordering, enum bl_atomic_op op) {
	switch (op) {
	case BL_ATOMIC_OP_XCHG:
		return atomic_exchange_explicit(ptr, val, (memory_order)ordering);
	case BL_ATOMIC_OP_ADD:
		return atomic_fetch_add_explicit(ptr, val, (memory_order)ordering);
	case BL_ATOMIC_OP_SUB:
		return atomic_fetch_sub_explicit(ptr, val, (memory_order)ordering);
	case BL_ATOMIC_OP_AND:
		return atomic_fetch_and_explicit(ptr, val, (memory_order)ordering);
	case BL_ATOMIC_OP_OR:
		return atomic_fetch_or_explicit(ptr, val, (memory_order)ordering);
	case BL_ATOMIC_OP_XOR:
		return atomic_fetch_xor_explicit(ptr, val, (memory_order)ordering);
	default:
		return 0;
	}
}

static inline s32 batomic_exchange_i32(batomic_s32 *ptr, s32 val, enum bl_memory_order ordering, enum bl_atomic_op op) {
	switch (op) {
	case BL_ATOMIC_OP_XCHG:
		return atomic_exchange_explicit(ptr, val, (memory_order)ordering);
	case BL_ATOMIC_OP_ADD:
		return atomic_fetch_add_explicit(ptr, val, (memory_order)ordering);
	case BL_ATOMIC_OP_SUB:
		return atomic_fetch_sub_explicit(ptr, val, (memory_order)ordering);
	case BL_ATOMIC_OP_AND:
		return atomic_fetch_and_explicit(ptr, val, (memory_order)ordering);
	case BL_ATOMIC_OP_OR:
		return atomic_fetch_or_explicit(ptr, val, (memory_order)ordering);
	case BL_ATOMIC_OP_XOR:
		return atomic_fetch_xor_explicit(ptr, val, (memory_order)ordering);
	default:
		return 0;
	}
}

static inline s64 batomic_exchange_i64(batomic_s64 *ptr, s64 val, enum bl_memory_order ordering, enum bl_atomic_op op) {
	switch (op) {
	case BL_ATOMIC_OP_XCHG:
		return atomic_exchange_explicit(ptr, val, (memory_order)ordering);
	case BL_ATOMIC_OP_ADD:
		return atomic_fetch_add_explicit(ptr, val, (memory_order)ordering);
	case BL_ATOMIC_OP_SUB:
		return atomic_fetch_sub_explicit(ptr, val, (memory_order)ordering);
	case BL_ATOMIC_OP_AND:
		return atomic_fetch_and_explicit(ptr, val, (memory_order)ordering);
	case BL_ATOMIC_OP_OR:
		return atomic_fetch_or_explicit(ptr, val, (memory_order)ordering);
	case BL_ATOMIC_OP_XOR:
		return atomic_fetch_xor_explicit(ptr, val, (memory_order)ordering);
	default:
		return 0;
	}
}

static inline bool batomic_cmpxchg_i8(batomic_s8 *ptr, s8 *expected, s8 desired, enum bl_memory_order success, enum bl_memory_order failure) {
	return atomic_compare_exchange_strong_explicit(ptr, expected, desired, (memory_order)success, (memory_order)failure);
}

static inline bool batomic_cmpxchg_i16(batomic_s16 *ptr, s16 *expected, s16 desired, enum bl_memory_order success, enum bl_memory_order failure) {
	return atomic_compare_exchange_strong_explicit(ptr, expected, desired, (memory_order)success, (memory_order)failure);
}

static inline bool batomic_cmpxchg_i32(batomic_s32 *ptr, s32 *expected, s32 desired, enum bl_memory_order success, enum bl_memory_order failure) {
	return atomic_compare_exchange_strong_explicit(ptr, expected, desired, (memory_order)success, (memory_order)failure);
}

static inline bool batomic_cmpxchg_i64(batomic_s64 *ptr, s64 *expected, s64 desired, enum bl_memory_order success, enum bl_memory_order failure) {
	return atomic_compare_exchange_strong_explicit(ptr, expected, desired, (memory_order)success, (memory_order)failure);
}

#endif

#endif