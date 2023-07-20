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

#include "save_data_file.h"

#include <gfx_utils.h>

void save_data_file_init(save_data_file_ctx_t *ctx, allocation_table_storage_ctx_t *base_storage, const char *path, hierarchical_save_file_table_ctx_t *file_table, uint64_t size, open_mode_t mode) {
    ctx->mode = mode;
    memcpy(&ctx->base_storage, base_storage, sizeof(allocation_table_storage_ctx_t));
    ctx->path = path;
    ctx->file_table = file_table;
    ctx->size = size;
}

bool save_data_file_validate_read_params(save_data_file_ctx_t *ctx, uint64_t *out_bytes_to_read, uint64_t offset, uint32_t size, open_mode_t open_mode) {
    *out_bytes_to_read = 0;

    if ((open_mode & OPEN_MODE_READ) == 0)
        return false;

    uint64_t file_size = ctx->size;

    if (offset > file_size)
        return false;

    *out_bytes_to_read = MIN(file_size - offset, size);

    return true;
}

bool save_data_file_validate_write_params(save_data_file_ctx_t *ctx, uint64_t offset, uint32_t size, open_mode_t open_mode, bool *out_is_resize_needed) {
    *out_is_resize_needed = false;

    if ((open_mode & OPEN_MODE_WRITE) == 0)
        return false;

    uint64_t file_size = ctx->size;

    if (offset + size > file_size) {
        *out_is_resize_needed = true;

        if ((open_mode & OPEN_MODE_ALLOW_APPEND) == 0)
            return false;
    }

    return true;
}

bool save_data_file_read(save_data_file_ctx_t *ctx, uint64_t *out_bytes_read, uint64_t offset, void *buffer, uint64_t count) {
    uint64_t to_read = 0;

    if (!save_data_file_validate_read_params(ctx, &to_read, offset, count, ctx->mode))
        return false;

    if (to_read == 0) {
        *out_bytes_read = 0;
        return true;
    }

    *out_bytes_read = save_allocation_table_storage_read(&ctx->base_storage, buffer, offset, to_read);
    return true;
}

bool save_data_file_write(save_data_file_ctx_t *ctx, uint64_t *out_bytes_written, uint64_t offset, const void *buffer, uint64_t count) {
    bool is_resize_needed;

    if (!save_data_file_validate_write_params(ctx, offset, count, ctx->mode, &is_resize_needed))
        return false;

    if (is_resize_needed) {
        if (!save_data_file_set_size(ctx, offset + count))
            return false;
    }

    *out_bytes_written = save_allocation_table_storage_write(&ctx->base_storage, buffer, offset, count);
    return true;
}

bool save_data_file_set_size(save_data_file_ctx_t *ctx, uint64_t size) {
    if (ctx->size == size)
        return true;

    save_allocation_table_storage_set_size(&ctx->base_storage, size);

    save_file_info_t file_info;
    if (!save_hierarchical_file_table_try_open_file(ctx->file_table, ctx->path, &file_info)) {
        EPRINTF("File not found!");
        return false;
    }

    file_info.start_block = ctx->base_storage.initial_block;
    fs_int64_set(&file_info.length, size);

    if (!save_hierarchical_file_table_add_file(ctx->file_table, ctx->path, &file_info))
        return false;

    ctx->size = size;

    return true;
}
