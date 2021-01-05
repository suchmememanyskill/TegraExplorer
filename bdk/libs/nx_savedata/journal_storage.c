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

#include "journal_storage.h"

#include "header.h"

#include <gfx_utils.h>
#include <mem/heap.h>

#include <string.h>

void save_journal_storage_init(journal_storage_ctx_t *ctx, substorage *base_storage, journal_header_t *header, journal_map_params_t *map_info) {
    memcpy(&ctx->base_storage, base_storage, sizeof(substorage));
    ctx->header = header;
    save_journal_map_init(&ctx->map, &header->map_header, map_info);
    ctx->block_size = (uint32_t)header->block_size;
    ctx->length = header->total_size - header->journal_size;
}

uint32_t save_journal_storage_read(journal_storage_ctx_t *ctx, void *buffer, uint64_t offset, uint64_t count) {
    uint64_t in_pos = offset;
    uint32_t out_pos = 0;
    uint32_t remaining = count;

    while (remaining) {
        uint32_t block_num = (uint32_t)(in_pos / ctx->block_size);
        uint32_t block_pos = (uint32_t)(in_pos % ctx->block_size);
        uint64_t physical_offset = ctx->map.entries[block_num].physical_index * ctx->block_size + block_pos;
        uint32_t bytes_to_read = MIN(ctx->block_size - block_pos, remaining);

        if (substorage_read(&ctx->base_storage, (uint8_t *)buffer + out_pos, physical_offset, bytes_to_read) != bytes_to_read)
            return 0;

        out_pos += bytes_to_read;
        in_pos += bytes_to_read;
        remaining -= bytes_to_read;
    }
    return out_pos;
}

uint32_t save_journal_storage_write(journal_storage_ctx_t *ctx, const void *buffer, uint64_t offset, uint64_t count) {
    uint64_t in_pos = offset;
    uint32_t out_pos = 0;
    uint32_t remaining = count;

    while (remaining) {
        uint32_t block_num = (uint32_t)(in_pos / ctx->block_size);
        uint32_t block_pos = (uint32_t)(in_pos % ctx->block_size);
        uint64_t physical_offset = ctx->map.entries[block_num].physical_index * ctx->block_size + block_pos;
        uint32_t bytes_to_write = MIN(ctx->block_size - block_pos, remaining);

        if (substorage_write(&ctx->base_storage, (uint8_t *)buffer + out_pos, physical_offset, bytes_to_write) != bytes_to_write)
            return 0;

        out_pos += bytes_to_write;
        in_pos += bytes_to_write;
        remaining -= bytes_to_write;
    }
    return out_pos;
}
