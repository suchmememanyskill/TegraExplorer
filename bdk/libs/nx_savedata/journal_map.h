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

#ifndef _JOURNAL_MAP_H_
#define _JOURNAL_MAP_H_

#include "storage.h"

#include <utils/types.h>

#include <stdint.h>

#define JOURNAL_MAP_ENTRY_SIZE 8

typedef struct {
    uint32_t version;
    uint32_t main_data_block_count;
    uint32_t journal_block_count;
    uint32_t _0x0C;
} journal_map_header_t;

typedef struct {
    uint8_t *map_storage;
    uint8_t *physical_block_bitmap;
    uint8_t *virtual_block_bitmap;
    uint8_t *free_block_bitmap;
} journal_map_params_t;

typedef struct {
    uint32_t physical_index;
    uint32_t virtual_index;
} journal_map_entry_t;

typedef struct {
    journal_map_header_t *header;
    journal_map_entry_t *entries;
    uint8_t *map_storage;
    uint8_t *modified_physical_blocks;
    uint8_t *modified_virtual_blocks;
    uint8_t *free_blocks;
} journal_map_ctx_t;

static ALWAYS_INLINE uint32_t save_journal_map_entry_make_physical_index(uint32_t index) {
    return index | 0x80000000;
}

static ALWAYS_INLINE uint32_t save_journal_map_entry_get_physical_index(uint32_t index) {
    return index & 0x7FFFFFFF;
}

void save_journal_map_init(journal_map_ctx_t *ctx, journal_map_header_t *header, journal_map_params_t *map_info);

#endif
