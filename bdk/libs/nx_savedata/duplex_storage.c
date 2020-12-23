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

#include "duplex_storage.h"

#include <gfx_utils.h>
#include <mem/heap.h>

void save_duplex_storage_init(duplex_storage_ctx_t *ctx, uint8_t *data_a, uint8_t *data_b, uint32_t block_size_power, void *bitmap, uint64_t bitmap_size) {
    substorage_init(&ctx->data_a, &memory_storage_vt, data_a, 0, ctx->_length);
    substorage_init(&ctx->data_b, &memory_storage_vt, data_b, 0, ctx->_length);
    substorage_init(&ctx->bitmap_storage, &memory_storage_vt, bitmap, 0, bitmap_size);
    ctx->block_size = 1 << block_size_power;

    ctx->bitmap.data = (uint8_t *)bitmap;
    ctx->bitmap.bitmap = malloc(bitmap_size >> 3);

    uint32_t bits_remaining = (uint32_t)bitmap_size;
    uint32_t bitmap_pos = 0;
    uint32_t *buffer_pos = (uint32_t *)ctx->bitmap.data;
    while (bits_remaining) {
        uint32_t bits_to_read = MIN(bits_remaining, 0x20);
        uint32_t val = *buffer_pos;
        for (uint32_t i = 0; i < bits_to_read; i++) {
            if (val & 0x80000000)
                save_bitmap_set_bit(ctx->bitmap.bitmap, bitmap_pos);
            else
                save_bitmap_clear_bit(ctx->bitmap.bitmap, bitmap_pos);
            bitmap_pos++;
            bits_remaining--;
            val <<= 1;
        }
        buffer_pos++;
    }
}

uint32_t save_duplex_storage_read(duplex_storage_ctx_t *ctx, void *buffer, uint64_t offset, uint64_t count) {
    uint64_t in_pos = offset;
    uint32_t out_pos = 0;
    uint32_t remaining = count;

    while (remaining) {
        uint32_t block_num = (uint32_t)(in_pos / ctx->block_size);
        uint32_t block_pos = (uint32_t)(in_pos % ctx->block_size);
        uint32_t bytes_to_read = MIN(ctx->block_size - block_pos, remaining);

        substorage *data = save_bitmap_check_bit(ctx->bitmap.bitmap, block_num) ? &ctx->data_b : &ctx->data_a;
        if (substorage_read(data, (uint8_t *)buffer + out_pos, in_pos, bytes_to_read) != bytes_to_read)
            return 0;

        out_pos += bytes_to_read;
        in_pos += bytes_to_read;
        remaining -= bytes_to_read;
    }
    return out_pos;
}

uint32_t save_duplex_storage_write(duplex_storage_ctx_t *ctx, const void *buffer, uint64_t offset, uint64_t count) {
    uint64_t in_pos = offset;
    uint32_t out_pos = 0;
    uint32_t remaining = count;

    while (remaining) {
        uint32_t block_num = (uint32_t)(in_pos / ctx->block_size);
        uint32_t block_pos = (uint32_t)(in_pos % ctx->block_size);
        uint32_t bytes_to_write = MIN(ctx->block_size - block_pos, remaining);

        substorage *data = save_bitmap_check_bit(ctx->bitmap.bitmap, block_num) ? &ctx->data_b : &ctx->data_a;
        if (substorage_write(data, (uint8_t *)buffer + out_pos, in_pos, bytes_to_write) != bytes_to_write)
            return 0;

        out_pos += bytes_to_write;
        in_pos += bytes_to_write;
        remaining -= bytes_to_write;
    }
    return out_pos;
}