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

#include "allocation_table_storage.h"

#include "allocation_table_iterator.h"

#include <gfx_utils.h>

bool save_allocation_table_storage_init(allocation_table_storage_ctx_t *ctx, substorage *data, allocation_table_ctx_t *table, uint32_t block_size, uint32_t initial_block) {
    ctx->base_storage = data;
    ctx->block_size = block_size;
    ctx->fat = table;
    ctx->initial_block = initial_block;
    ctx->_length = 0;
    if (initial_block != 0xFFFFFFFF) {
        uint32_t list_length = save_allocation_table_get_list_length(table, initial_block);
        if (list_length == 0) {
            EPRINTF("Allocation table storage init failed!");
            return false;
        };
        ctx->_length = list_length * block_size;
    }
    return true;
}

uint32_t save_allocation_table_storage_read(allocation_table_storage_ctx_t *ctx, void *buffer, uint64_t offset, uint64_t count) {
    allocation_table_iterator_ctx_t iterator;
    if (!save_allocation_table_iterator_begin(&iterator, ctx->fat, ctx->initial_block))
        return 0;
    uint64_t in_pos = offset;
    uint32_t out_pos = 0;
    uint32_t remaining = count;

    while (remaining) {
        uint32_t block_num = (uint32_t)(in_pos / ctx->block_size);
        if (!save_allocation_table_iterator_seek(&iterator, block_num)) {
            EPRINTFARGS("Invalid allocation table offset: %x", (uint32_t)offset);
            return 0;
        }

        uint32_t segment_pos = (uint32_t)(in_pos - (uint64_t)iterator.virtual_block * ctx->block_size);
        uint64_t physical_offset = iterator.physical_block * ctx->block_size + segment_pos;

        uint32_t remaining_in_segment = iterator.current_segment_size * ctx->block_size - segment_pos;
        uint32_t bytes_to_read = MIN(remaining, remaining_in_segment);

        if (substorage_read(ctx->base_storage, (uint8_t *)buffer + out_pos, physical_offset, bytes_to_read) != bytes_to_read)
            return 0;

        out_pos += bytes_to_read;
        in_pos += bytes_to_read;
        remaining -= bytes_to_read;
    }
    return out_pos;
}

uint32_t save_allocation_table_storage_write(allocation_table_storage_ctx_t *ctx, const void *buffer, uint64_t offset, uint64_t count) {
    allocation_table_iterator_ctx_t iterator;
    if (!save_allocation_table_iterator_begin(&iterator, ctx->fat, ctx->initial_block))
        return 0;
    uint64_t in_pos = offset;
    uint32_t out_pos = 0;
    uint32_t remaining = count;

    while (remaining) {
        uint32_t block_num = (uint32_t)(in_pos / ctx->block_size);
        if (!save_allocation_table_iterator_seek(&iterator, block_num)) {
            EPRINTFARGS("Invalid allocation table offset: %x", (uint32_t)offset);
            return 0;
        }

        uint32_t segment_pos = (uint32_t)(in_pos - (uint64_t)iterator.virtual_block * ctx->block_size);
        uint64_t physical_offset = iterator.physical_block * ctx->block_size + segment_pos;

        uint32_t remaining_in_segment = iterator.current_segment_size * ctx->block_size - segment_pos;
        uint32_t bytes_to_write = MIN(remaining, remaining_in_segment);


        if (substorage_write(ctx->base_storage, (uint8_t *)buffer + out_pos, physical_offset, bytes_to_write) != bytes_to_write)
            return 0;

        out_pos += bytes_to_write;
        in_pos += bytes_to_write;
        remaining -= bytes_to_write;
    }
    return out_pos;
}

bool save_allocation_table_storage_set_size(allocation_table_storage_ctx_t *ctx, uint64_t size) {
    uint32_t old_block_count = (uint32_t)DIV_ROUND_UP(ctx->_length, ctx->block_size);
    uint32_t new_block_count = (uint32_t)DIV_ROUND_UP(size, ctx->block_size);

    if (old_block_count == new_block_count)
        return true;

    if (old_block_count == 0) {
        ctx->initial_block = save_allocation_table_allocate(ctx->fat, new_block_count);
        if (ctx->initial_block == 0xFFFFFFFF) {
            EPRINTF("Not enough space to resize file!");
            return false;
        }
        ctx->_length = new_block_count * ctx->block_size;
        return true;
    }

    if (new_block_count == 0) {
        save_allocation_table_free(ctx->fat, ctx->initial_block);

        ctx->initial_block = 0x80000000;
        ctx->_length = 0;
        return true;
    }

    if (new_block_count > old_block_count) {
        uint32_t new_blocks = save_allocation_table_allocate(ctx->fat, new_block_count - old_block_count);
        if (new_blocks == 0xFFFFFFFF) {
            EPRINTF("Not enough space to resize file!");
            return false;
        }
        if (!save_allocation_table_join(ctx->fat, ctx->initial_block, new_blocks))
            return false;
    } else {
        uint32_t old_blocks = save_allocation_table_trim(ctx->fat, ctx->initial_block, new_block_count);
        if (old_blocks == 0xFFFFFFFF) {
            EPRINTF("Failure to trim!");
            return false;
        }
        if (!save_allocation_table_free(ctx->fat, old_blocks))
            return false;
    }

    ctx->_length = new_block_count * ctx->block_size;

    return true;
}
