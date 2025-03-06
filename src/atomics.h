#ifndef BL_ATOMICS_H
#define BL_ATOMICS_H

// Atomics
#if BL_PLATFORM_WIN
#include <Windows.h>

#define batomic_store_s32(a, val)     InterlockedExchange((a), (val));
#define batomic_load_s32(a)           InterlockedCompareExchange((a), 0, 0)
#define batomic_fetch_add_s32(a, val) InterlockedAdd((a), (val))
#define batomic_fetch_add_u32(a, val) (u32) InterlockedAdd((volatile LONG *)(a), (LONG)(val))
#define batomic_store_s64(a, val)     InterlockedExchange64((a), (val));
#define batomic_load_s64(a)           InterlockedCompareExchange64((a), 0, 0)
#define batomic_fetch_add_s64(a, val) InterlockedAdd64((a), (val))

typedef volatile LONG   batomic_s32;
typedef volatile ULONG  batomic_u32;
typedef volatile LONG64 batomic_s64;

#else
#include <stdatomic.h>

#define batomic_store_s32(a, val)     atomic_store((a), (val))
#define batomic_load_s32(a)           atomic_load((a))
#define batomic_fetch_add_s32(a, val) atomic_fetch_add((a), (val))
#define batomic_fetch_add_u32(a, val) atomic_fetch_add((a), (val))
#define batomic_store_s64(a, val)     atomic_store((a), (val))
#define batomic_load_s64(a)           atomic_load((a))
#define batomic_fetch_add_s64(a, val) atomic_fetch_add((a), (val))

typedef atomic_int   batomic_s32;
typedef atomic_uint  batomic_u32;
typedef atomic_llong batomic_s64;

#endif

#endif
