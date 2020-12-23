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

#include "journal_map.h"

#include "journal_storage.h"
#include "storage.h"

#include <gfx_utils.h>
#include <mem/heap.h>

static journal_map_entry_t *read_map_entries(uint8_t *map_table, uint32_t count) {
    journal_map_entry_t *reader = (journal_map_entry_t *)map_table;
    journal_map_entry_t *map = malloc(count * sizeof(journal_map_entry_t));

    for (uint32_t i = 0; i < count; i++) {
        map[i].virtual_index = i;
        map[i].physical_index = save_journal_map_entry_get_physical_index(reader->physical_index);
        reader++;
    }

    return map;
}

void save_journal_map_init(journal_map_ctx_t *ctx, journal_map_header_t *header, journal_map_params_t *map_info) {
    ctx->header = header;
    ctx->map_storage = map_info->map_storage;
    ctx->modified_physical_blocks = map_info->physical_block_bitmap;
    ctx->modified_virtual_blocks = map_info->virtual_block_bitmap;
    ctx->free_blocks = map_info->free_block_bitmap;
    ctx->entries = read_map_entries(ctx->map_storage, header->main_data_block_count);
}
