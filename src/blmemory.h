#ifndef BL_BLMEMORY_H
#define BL_BLMEMORY_H

#include "basic_types.h"
#include "config.h"

#define bmalloc(size)       bl_malloc_impl(size, __FILE__, __LINE__)
#define brealloc(ptr, size) bl_realloc_impl(ptr, size, __FILE__, __LINE__)
#define bfree(ptr)          bl_free_impl(ptr, __FILE__, __LINE__)

// These might not be implemented in some cases...
void bl_alloc_init(void);
void bl_alloc_terminate(void);
void bl_alloc_thread_init(void);
void bl_alloc_thread_terminate(void);

void *bl_realloc_impl(void *ptr, const size_t size, const char *filename, s32 line);
void *bl_malloc_impl(const size_t size, const char *filename, s32 line);
void  bl_free_impl(void *ptr, const char *filename, s32 line);
void *bl_zeromem(void *dest, usize size);

#endif // BL_BLMEMORY_H
