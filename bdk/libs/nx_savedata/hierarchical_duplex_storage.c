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

#include "hierarchical_duplex_storage.h"

#include <gfx_utils.h>
#include <mem/heap.h>

void save_duplex_fs_layer_info_init(duplex_fs_layer_info_t *ctx, uint8_t *data_a, uint8_t *data_b, duplex_info_t *info) {
    if (data_a)
        ctx->data_a = data_a;
    if (data_b)
        ctx->data_b = data_b;
    ctx->info.offset = info->offset;
    ctx->info.length = info->length;
    ctx->info.block_size_power = info->block_size_power;
}

bool save_hierarchical_duplex_storage_init(hierarchical_duplex_storage_ctx_t *ctx, remap_storage_ctx_t *storage, save_header_t *header) {
    substorage base_storage;
    substorage_init(&base_storage, &remap_storage_vt, storage, 0, -1);
    fs_layout_t *layout = &header->layout;
    duplex_fs_layer_info_t duplex_layers[3];

    save_duplex_fs_layer_info_init(&duplex_layers[0], (uint8_t *)header + layout->duplex_master_offset_a, (uint8_t *)header + layout->duplex_master_offset_b, &header->duplex_header.layers[0]);

    duplex_layers[1].data_a = malloc(layout->duplex_l1_size);
    duplex_layers[1].data_b = malloc(layout->duplex_l1_size);
    if (substorage_read(&base_storage, duplex_layers[1].data_a, layout->duplex_l1_offset_a, layout->duplex_l1_size) != layout->duplex_l1_size) {
        EPRINTF("Hier dup init: Failed to read L1 bitmap A!");
        return false;
    }
    if (substorage_read(&base_storage, duplex_layers[1].data_b, layout->duplex_l1_offset_b, layout->duplex_l1_size) != layout->duplex_l1_size) {
        EPRINTF("Hier dup init: Failed to read L1 bitmap B!");
        return false;
    }
    save_duplex_fs_layer_info_init(&duplex_layers[1], NULL, NULL, &header->duplex_header.layers[1]);

    duplex_layers[2].data_a = malloc(layout->duplex_data_size);
    duplex_layers[2].data_b = malloc(layout->duplex_data_size);
    if (substorage_read(&base_storage, duplex_layers[2].data_a, layout->duplex_data_offset_a, layout->duplex_data_size) != layout->duplex_data_size) {
        EPRINTF("Hier dup init: Failed to read duplex data A!");
        return false;
    }
    if (substorage_read(&base_storage, duplex_layers[2].data_b, layout->duplex_data_offset_b, layout->duplex_data_size) != layout->duplex_data_size) {
        EPRINTF("Hier dup init: Failed to read duplex data B!");
        return false;
    }
    save_duplex_fs_layer_info_init(&duplex_layers[2], NULL, NULL, &header->duplex_header.layers[2]);

    uint8_t *bitmap = layout->duplex_index == 1 ? duplex_layers[0].data_b : duplex_layers[0].data_a;
    ctx->layers[0]._length = layout->duplex_l1_size;
    save_duplex_storage_init(&ctx->layers[0], duplex_layers[1].data_a, duplex_layers[1].data_b, duplex_layers[1].info.block_size_power, bitmap, layout->duplex_master_size);

    bitmap = malloc(ctx->layers[0]._length);
    if (save_duplex_storage_read(&ctx->layers[0], bitmap, 0, ctx->layers[0]._length) != ctx->layers[0]._length) {
        EPRINTF("Hier dup init: Failed to read bitmap!");
        return false;
    }
    ctx->layers[1]._length = layout->duplex_data_size;
    save_duplex_storage_init(&ctx->layers[1], duplex_layers[2].data_a, duplex_layers[2].data_b, duplex_layers[2].info.block_size_power, bitmap, ctx->layers[0]._length);

    ctx->data_layer = &ctx->layers[1];
    ctx->_length = ctx->data_layer->_length;

    return true;
}

bool save_hierarchical_duplex_storage_flush(hierarchical_duplex_storage_ctx_t *ctx, remap_storage_ctx_t *storage, save_header_t *header) {
    substorage base_storage;
    substorage_init(&base_storage, &remap_storage_vt, storage, 0, -1);
    fs_layout_t *layout = &header->layout;

    if (save_duplex_storage_write(&ctx->layers[0], &ctx->layers[1].bitmap.data, 0, ctx->layers[0]._length) != ctx->layers[0]._length) {
        EPRINTF("Hier dup flush: Failed to write bitmap!");
        return false;
    }

    if (substorage_write(&base_storage, ctx->layers[1].data_a.base_storage.ctx, layout->duplex_data_offset_a, layout->duplex_data_size) != layout->duplex_data_size) {
        EPRINTF("Hier dup flush: Failed to write data A!");
        return false;
    }
    if (substorage_write(&base_storage, ctx->layers[1].data_b.base_storage.ctx, layout->duplex_data_offset_b, layout->duplex_data_size) != layout->duplex_data_size) {
        EPRINTF("Hier dup flush: Failed to write data B!");
        return false;
    }

    return true;
}
