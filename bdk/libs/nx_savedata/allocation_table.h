/*
 * Copyright (c) 2019-2020 shchmue
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
ISC License

hactool Copyright (c) 2018, SciresM

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#ifndef _ALLOCATION_TABLE_H_
#define _ALLOCATION_TABLE_H_

#include "storage.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#define SAVE_FAT_ENTRY_SIZE 8

typedef struct {
    uint64_t offset;
    uint32_t count;
    uint32_t _0xC;
} storage_info_t;

typedef struct {
    uint64_t block_size;
    storage_info_t fat_storage_info;
    storage_info_t data_storage_info;
    uint32_t directory_table_block;
    uint32_t file_table_block;
} allocation_table_header_t;

static_assert(sizeof(allocation_table_header_t) == 0x30, "Allocation table header size is wrong!");

typedef struct {
    uint32_t prev;
    uint32_t next;
} allocation_table_entry_t;

typedef struct {
    uint32_t free_list_entry_index;
    void *base_storage;
    allocation_table_header_t *header;
} allocation_table_ctx_t;

static ALWAYS_INLINE uint32_t allocation_table_entry_index_to_block(uint32_t entry_index) {
    return entry_index - 1;
}

static ALWAYS_INLINE uint32_t allocation_table_block_to_entry_index(uint32_t block_index) {
    return block_index + 1;
}

static ALWAYS_INLINE int allocation_table_get_prev(allocation_table_entry_t *entry) {
    return entry->prev & 0x7FFFFFFF;
}

static ALWAYS_INLINE int allocation_table_get_next(allocation_table_entry_t *entry) {
    return entry->next & 0x7FFFFFFF;
}

static ALWAYS_INLINE int allocation_table_is_list_start(allocation_table_entry_t *entry) {
    return entry->prev == 0x80000000;
}

static ALWAYS_INLINE int allocation_table_is_list_end(allocation_table_entry_t *entry) {
    return (entry->next & 0x7FFFFFFF) == 0;
}

static ALWAYS_INLINE bool allocation_table_is_multi_block_segment(allocation_table_entry_t *entry) {
    return entry->next & 0x80000000;
}

static ALWAYS_INLINE void allocation_table_make_multi_block_segment(allocation_table_entry_t *entry) {
    entry->next |= 0x80000000;
}

static ALWAYS_INLINE void allocation_table_make_single_block_segment(allocation_table_entry_t *entry) {
    entry->next &= 0x7FFFFFFF;
}

static ALWAYS_INLINE bool allocation_table_is_single_block_segment(allocation_table_entry_t *entry) {
    return (entry->next & 0x80000000) == 0;
}

static ALWAYS_INLINE void allocation_table_make_list_start(allocation_table_entry_t *entry) {
    entry->prev = 0x80000000;
}

static ALWAYS_INLINE bool allocation_table_is_range_entry(allocation_table_entry_t *entry) {
    return (entry->prev & 0x80000000) == 0x80000000 && entry->prev != 0x80000000;
}

static ALWAYS_INLINE void allocation_table_make_range_entry(allocation_table_entry_t *entry) {
    entry->prev |= 0x80000000;
}

static ALWAYS_INLINE void allocation_table_set_next(allocation_table_entry_t *entry, int val) {
    entry->next = (entry->next & 0x80000000) | val;
}

static ALWAYS_INLINE void allocation_table_set_prev(allocation_table_entry_t *entry, int val) {
    entry->prev = val;
}

static ALWAYS_INLINE void allocation_table_set_range(allocation_table_entry_t *entry, int start_index, int end_index) {
    entry->next = end_index;
    entry->prev = start_index;
    allocation_table_make_range_entry(entry);
}

static ALWAYS_INLINE uint64_t allocation_table_query_size(uint32_t block_count) {
    return SAVE_FAT_ENTRY_SIZE * allocation_table_block_to_entry_index(block_count);
}

static ALWAYS_INLINE allocation_table_entry_t *save_allocation_table_read_entry(allocation_table_ctx_t *ctx, uint32_t entry_index) {
    return (allocation_table_entry_t *)((uint8_t *)ctx->base_storage + entry_index * SAVE_FAT_ENTRY_SIZE);
}

static ALWAYS_INLINE void save_allocation_table_write_entry(allocation_table_ctx_t *ctx, uint32_t entry_index, allocation_table_entry_t *entry) {
    memcpy((uint8_t *)ctx->base_storage + entry_index * SAVE_FAT_ENTRY_SIZE, entry, SAVE_FAT_ENTRY_SIZE);
}

static ALWAYS_INLINE uint32_t save_allocation_table_get_free_list_entry_index(allocation_table_ctx_t *ctx) {
    return allocation_table_get_next(save_allocation_table_read_entry(ctx, ctx->free_list_entry_index));
}

static ALWAYS_INLINE uint32_t save_allocation_table_get_free_list_block_index(allocation_table_ctx_t *ctx) {
    return allocation_table_entry_index_to_block(save_allocation_table_get_free_list_entry_index(ctx));
}

void save_allocation_table_init(allocation_table_ctx_t *ctx, void *storage, allocation_table_header_t *header);
void save_allocation_table_set_free_list_entry_index(allocation_table_ctx_t *ctx, uint32_t head_block_index);
void save_allocation_table_set_free_list_block_index(allocation_table_ctx_t *ctx, uint32_t head_block_index);
bool save_allocation_table_split(allocation_table_ctx_t *ctx, uint32_t segment_block_index, uint32_t first_sub_segment_length);
uint32_t save_allocation_table_trim(allocation_table_ctx_t *ctx, uint32_t list_head_block_index, uint32_t new_list_length);
bool save_allocation_table_join(allocation_table_ctx_t *ctx, uint32_t front_list_block_index, uint32_t back_list_block_index);
uint32_t save_allocation_table_allocate(allocation_table_ctx_t *ctx, uint32_t block_count);
bool save_allocation_table_free(allocation_table_ctx_t *ctx, uint32_t list_block_index);
uint32_t save_allocation_table_read_entry_with_length(allocation_table_ctx_t *ctx, uint32_t block_index, allocation_table_entry_t *entry);
uint32_t save_allocation_table_get_list_length(allocation_table_ctx_t *ctx, uint32_t block_index);
uint32_t save_allocation_table_get_free_list_length(allocation_table_ctx_t *ctx);

#endif
