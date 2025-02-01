// =================================================================================================
// bl
//
// File:   table.c
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

#include "table.h"

#define DEFAULT_SLOT_COUNT 128
#define DEFAULT_ELEM_COUNT 64
#define LOAD_FACTOR        70
#define EMPTY_INDEX        0
#define DELETED_INDEX      -1

#define HASH_T BL_TBL_HASH_T

struct slot {
	HASH_T hash;
	s32    index;
};

struct header {
	struct slot *slots;
	u32          slots_num, len, allocated;
	u32          _; // Padding
	u8           data[];
};

static void    resize(struct header *tbl, u32 new_size);
static u32     find_free_slot_index(struct header *tbl, HASH_T hash);
static u32     initial_increment(struct header *tbl, HASH_T hash);
static bool    lookup_indices(struct header *tbl, HASH_T hash, str_t key, u32 entry_size, s32 data_key_offset, u32 *out_slot_index, u32 *out_entry_index);
struct header *ensure_capacity(struct header *tbl, u32 elem_size, u32 elem_count);

static inline struct header *get_header(void *ptr) {
	struct header *tbl = ptr;
	return tbl ? tbl - 1 : NULL;
}

void *_tbl_init(void *ptr, u32 elem_size, u32 elem_count) {
	struct header *tbl = get_header(ptr);
	bassert(tbl == NULL && "Table already allocated.");
	elem_count = MAX(elem_count, DEFAULT_ELEM_COUNT);
	tbl        = ensure_capacity(tbl, elem_size, elem_count);
	resize(tbl, next_pow_2(elem_count * 2));
	return tbl->data;
}

void _tbl_free(void *ptr) {
	struct header *tbl = get_header(ptr);
	if (!tbl) return;
	bfree(tbl->slots);
	bfree(tbl);
}

void _tbl_clear(void *ptr) {
	struct header *tbl = get_header(ptr);
	if (!tbl) return;
	bl_zeromem(tbl->slots, sizeof(struct slot) * tbl->slots_num);
	tbl->len = 0;
}

void *_tbl_insert(void *ptr, HASH_T hash, void *elem_data, u32 elem_size) {
	struct header *tbl = get_header(ptr);
	tbl                = ensure_capacity(tbl, elem_size, tbl ? tbl->len + 1 : DEFAULT_ELEM_COUNT);
	bassert(tbl);

	memcpy(tbl->data + (tbl->len * elem_size), elem_data, elem_size);
	tbl->len += 1;

	if (!tbl->slots) resize(tbl, DEFAULT_SLOT_COUNT);
	const u32 slot_index         = find_free_slot_index(tbl, hash);
	tbl->slots[slot_index].index = tbl->len;
	tbl->slots[slot_index].hash  = hash;

	return tbl->data;
}

s32 _tbl_lookup_index(void *ptr, HASH_T hash, str_t key, u32 entry_size, s32 data_key_offset) {
	struct header *tbl = get_header(ptr);
	if (!tbl) return -1;

	u32 slot_index, entry_index;
	if (lookup_indices(tbl, hash, key, entry_size, data_key_offset, &slot_index, &entry_index)) {
		bassert(entry_index >= 0 && entry_index < tbl->len);
		return (s32)entry_index;
	}
	return -1;
}

bool _tbl_erase(void *ptr, BL_TBL_HASH_T hash, str_t key, u32 entry_size, u32 hash_size, s32 data_hash_offset, s32 data_key_offset) {
	bassert(hash_size > 0 && hash_size <= sizeof(HASH_T));

	struct header *tbl = get_header(ptr);
	if (!tbl) return false;

	u32 erase_slot_index, erase_entry_index;
	if (!lookup_indices(tbl, hash, key, entry_size, data_key_offset, &erase_slot_index, &erase_entry_index)) {
		return false;
	}
	bassert(erase_entry_index >= 0 && erase_entry_index < tbl->len);
	bassert(erase_slot_index >= 0 && erase_slot_index < tbl->slots_num);
	if (tbl->len > 1 && (erase_entry_index + 1) != tbl->len) {
		// We will swap erased element with the last element in the array and reduce len by one. So we also have to
		// remap slot index for the last element here.
		HASH_T last_entry_hash = 0;
		memcpy(&last_entry_hash, (void *)(tbl->data + (tbl->len - 1) * entry_size + data_hash_offset), hash_size);
		const str_t last_entry_key = data_key_offset == -1 ? (str_t){0} : (*(str_t *)(tbl->data + (tbl->len - 1) * entry_size + data_key_offset));

		u32 last_slot_index  = 0;
		u32 last_entry_index = 0;
		if (!lookup_indices(tbl, last_entry_hash, last_entry_key, entry_size, data_key_offset, &last_slot_index, &last_entry_index)) {
			bassert(false && "Cannot find the last table element!");
		}
		bassert(last_slot_index != erase_slot_index);
		bassert(last_entry_index != erase_entry_index);
		bassert(last_entry_index == tbl->len - 1);
		tbl->slots[last_slot_index].index = erase_entry_index + 1;
		memcpy(&tbl->data[erase_entry_index * entry_size], &tbl->data[last_entry_index * entry_size], entry_size);
	}
	bassert(tbl->len > 0);
	tbl->len -= 1;
	tbl->slots[erase_slot_index].index = DELETED_INDEX;
	return true;
}

u32 _tbl_len(void *ptr) {
	struct header *tbl = get_header(ptr);
	if (!tbl) return 0;
	return tbl->len;
}

struct header *ensure_capacity(struct header *tbl, u32 elem_size, u32 elem_count) {
	bassert(elem_count > 0);
	if (tbl && tbl->allocated >= elem_count) return tbl;

	elem_count             = MAX(elem_count, tbl ? (tbl->allocated * 2) : 0);
	const u32      size    = elem_size * elem_count + sizeof(struct header);
	struct header *new_tbl = brealloc(tbl, size);
	if (!tbl) bl_zeromem(new_tbl, sizeof(struct header));
	new_tbl->allocated = elem_count;
	return new_tbl;
}

u32 initial_increment(struct header *tbl, HASH_T hash) {
	bassert(tbl);
	bassert(tbl->slots_num > 0);
	return (u32)(1 + hash % (HASH_T)(tbl->slots_num - 1));
}

u32 find_free_slot_index(struct header *tbl, HASH_T hash) {
	bassert(tbl);
	bassert(tbl->slots && tbl->slots_num > 0);

	if ((tbl->len + 1) * 100 >= tbl->slots_num * LOAD_FACTOR) {
		resize(tbl, next_pow_2(tbl->slots_num * 2));
	}
	u32 index     = (u32)(hash % (HASH_T)tbl->slots_num);
	u32 increment = initial_increment(tbl, hash);
	while (1) {
		const s32 entry_index = tbl->slots[index].index;
		if (entry_index == EMPTY_INDEX || entry_index == DELETED_INDEX) {
			break;
		}
		index += increment;
		increment += 1;
		while (index >= tbl->slots_num)
			index -= tbl->slots_num;
	}
	bassert(index < tbl->slots_num);
	return index;
}

void resize(struct header *tbl, u32 new_size) {
	bassert(tbl);
	bassert(new_size > 32);
	bassert(new_size != tbl->slots_num);
	struct slot *old_slots     = tbl->slots;
	const s32    old_slots_num = tbl->slots_num;

	tbl->slots     = bmalloc(sizeof(struct slot) * new_size);
	tbl->slots_num = new_size;

	bl_zeromem(tbl->slots, sizeof(struct slot) * tbl->slots_num); // Valid until EMPTY_INDEX == 0

	s32 used_count = tbl->len;
	for (int i = 0; i < old_slots_num && used_count >= 0; ++i) {
		struct slot slot = old_slots[i];
		if (slot.index == EMPTY_INDEX || slot.index == DELETED_INDEX) continue;
		u32 new_slot_index               = find_free_slot_index(tbl, slot.hash);
		tbl->slots[new_slot_index].index = slot.index;
		tbl->slots[new_slot_index].hash  = slot.hash;
		--used_count;
	}

	bfree(old_slots);
}

bool lookup_indices(struct header *tbl, HASH_T hash, str_t key, u32 entry_size, s32 data_key_offset, u32 *out_slot_index, u32 *out_entry_index) {
	bassert(tbl);
	u32 index     = (u32)(hash % (HASH_T)tbl->slots_num);
	u32 increment = initial_increment(tbl, hash);
	while (1) {
		struct slot slot = tbl->slots[index];
		if (slot.index == EMPTY_INDEX) break;
		if (slot.hash == hash) {
			if (data_key_offset == -1) {
				if (slot.index == DELETED_INDEX) return false;
				*out_slot_index  = index;
				*out_entry_index = slot.index - 1;
				return true;
			}

			if (slot.index != DELETED_INDEX) {
				bassert(slot.index > 0);
				const u32 entry_index = slot.index - 1;
				str_t    *entry_str   = (str_t *)(tbl->data + entry_index * entry_size + data_key_offset);
				// 2024-08-09 This is be actually fast in cases where strings has different len.
				if (str_match(*entry_str, key)) {
					*out_slot_index  = index;
					*out_entry_index = entry_index;
					return true;
				}
			}
		}
		index += increment;
		increment += 1;
		while (index >= tbl->slots_num)
			index -= tbl->slots_num;
	}
	return false;
}
