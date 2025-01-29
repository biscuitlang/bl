// =================================================================================================
// bl
//
// File:   table.h
// Author: Martin Dorazil
// Date:   2024-08-09
//
// Copyright 2024 Martin Dorazil
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

#ifndef BL_TABLE_H
#define BL_TABLE_H

#define BL_TBL_HASH_T u64

BL_STATIC_ASSERT(sizeof(BL_TBL_HASH_T) == sizeof(void *), "Table hash type should have same size as pointer!");

void *_tbl_init(void *ptr, u32 elem_size, u32 elem_count);
void  _tbl_free(void *ptr);
void *_tbl_insert(void *ptr, BL_TBL_HASH_T hash, void *elem_data, u32 elem_size);
s32   _tbl_lookup_index(void *ptr, BL_TBL_HASH_T hash, str_t key, u32 entry_size, s32 data_key_offset);
void  _tbl_clear(void *ptr);
u32   _tbl_len(void *ptr);
bool  _tbl_erase(void *ptr, BL_TBL_HASH_T hash, str_t key, u32 entry_size, u32 hash_size, s32 data_hash_offset, s32 data_key_offset);

#define tbl_init(tbl, elem_count)                               \
	{                                                           \
		(tbl) = _tbl_init((tbl), sizeof(*(tbl)), (elem_count)); \
	}                                                           \
	(void)0

#define tbl_insert(tbl, entry)                                                             \
	{                                                                                      \
		(tbl) = _tbl_insert((tbl), (BL_TBL_HASH_T)(entry).hash, &(entry), sizeof(*(tbl))); \
	}                                                                                      \
	(void)0

#define tbl_free(tbl)     \
	{                     \
		_tbl_free((tbl)); \
		(tbl) = NULL;     \
	}                     \
	(void)0

#define tbl_clear(tbl)     \
	{                      \
		_tbl_clear((tbl)); \
	}                      \
	(void)0

#define tbl_key_offset(tbl)                       ((s32)(((u8 *)(&((tbl)->key))) - ((u8 *)(tbl))))
#define tbl_hash_offset(tbl)                      ((s32)(((u8 *)(&((tbl)->hash))) - ((u8 *)(tbl))))
#define tbl_lookup_index_with_key(tbl, hash, key) _tbl_lookup_index((tbl), (hash), (key), sizeof(*(tbl)), tbl_key_offset(tbl))
#define tbl_lookup_index(tbl, hash)               _tbl_lookup_index((tbl), (BL_TBL_HASH_T)(hash), (str_t){0}, sizeof(*(tbl)), -1)
#define tbl_erase_with_key(tbl, hash, key)        _tbl_erase((tbl), (hash), (key), sizeof(*(tbl)), sizeof(hash), tbl_hash_offset(tbl), tbl_key_offset(tbl))
#define tbl_erase(tbl, hash)                      _tbl_erase((tbl), (BL_TBL_HASH_T)(hash), (str_t){0}, sizeof(*(tbl)), sizeof(hash), tbl_hash_offset(tbl), -1)
#define tbl_len(tbl)                              _tbl_len((tbl))

#endif
