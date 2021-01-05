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

#include "save_data_file_system_core.h"

#include "allocation_table_storage.h"
#include "header.h"
#include "save.h"

#include <gfx_utils.h>
#include <mem/heap.h>

static ALWAYS_INLINE bool save_data_file_system_core_open_fat_storage(save_data_file_system_core_ctx_t *ctx, allocation_table_storage_ctx_t *storage_ctx, uint32_t block_index) {
    return save_allocation_table_storage_init(storage_ctx, ctx->base_storage, &ctx->allocation_table, (uint32_t)ctx->header->block_size, block_index);
}

bool save_data_file_system_core_init(save_data_file_system_core_ctx_t *ctx, substorage *storage, void *allocation_table, save_fs_header_t *save_fs_header) {
    save_allocation_table_init(&ctx->allocation_table, allocation_table, &save_fs_header->fat_header);
    ctx->header = save_fs_header;
    ctx->base_storage = storage;

    save_filesystem_list_ctx_t *dir_table = &ctx->file_table.directory_table;
    save_filesystem_list_ctx_t *file_table = &ctx->file_table.file_table;
    if (!save_data_file_system_core_open_fat_storage(ctx, &dir_table->storage, save_fs_header->fat_header.directory_table_block)) {
        EPRINTF("Failed to init dir table for fs core!");
        return false;
    }
    if (!save_data_file_system_core_open_fat_storage(ctx, &file_table->storage, save_fs_header->fat_header.file_table_block)) {
        EPRINTF("Failed to init file table for fs core!");
        return false;
    }
    save_fs_list_init(dir_table);
    save_fs_list_init(file_table);

    return true;
}

bool save_data_file_system_core_create_directory(save_data_file_system_core_ctx_t *ctx, const char *path) {
    return save_hierarchical_file_table_add_directory(&ctx->file_table, path);
}

bool save_data_file_system_core_create_file(save_data_file_system_core_ctx_t *ctx, const char *path, uint64_t size) {
    if (size == 0) {
        save_file_info_t empty_file_entry = {0x80000000, {0}, {0}};
        save_hierarchical_file_table_add_file(&ctx->file_table, path, &empty_file_entry);
        return true;
    }

    uint32_t block_count = (uint32_t)DIV_ROUND_UP(size, ctx->allocation_table.header->block_size);
    uint32_t start_block = save_allocation_table_allocate(&ctx->allocation_table, block_count);

    if (start_block == 0xFFFFFFFF)
        return false;

    save_file_info_t file_entry = {start_block, {0}, {0}};
    fs_int64_set(&file_entry.length, size);
    return save_hierarchical_file_table_add_file(&ctx->file_table, path, &file_entry);
}

bool save_data_file_system_core_delete_directory(save_data_file_system_core_ctx_t *ctx, const char *path) {
    return save_hierarchical_file_table_delete_directory(&ctx->file_table, path);
}

bool save_data_file_system_core_delete_file(save_data_file_system_core_ctx_t *ctx, const char *path) {
    save_file_info_t file_info;
    if (!save_hierarchical_file_table_try_open_file(&ctx->file_table, path, &file_info))
        return false;

    if (file_info.start_block != 0x80000000) {
        save_allocation_table_free(&ctx->allocation_table, file_info.start_block);
    }

    save_hierarchical_file_table_delete_file(&ctx->file_table, path);

    return true;
}

bool save_data_file_system_core_open_directory(save_data_file_system_core_ctx_t *ctx, save_data_directory_ctx_t *directory, const char *path, open_directory_mode_t mode) {
    memset(directory, 0, sizeof(save_data_directory_ctx_t));

    save_find_position_t position;
    if (!save_hierarchical_file_table_try_open_directory(&ctx->file_table, path, &position))
        return false;

    save_data_directory_init(directory, &ctx->file_table, &position, mode);

    return true;
}

bool save_data_file_system_core_open_file(save_data_file_system_core_ctx_t *ctx, save_data_file_ctx_t *file, const char *path, open_mode_t mode) {
    memset(file, 0, sizeof(save_data_file_ctx_t));

    save_file_info_t file_info;
    if (!save_hierarchical_file_table_try_open_file(&ctx->file_table, path, &file_info))
        return false;

    allocation_table_storage_ctx_t storage;
    save_data_file_system_core_open_fat_storage(ctx, &storage, file_info.start_block);

    save_data_file_init(file, &storage, path, &ctx->file_table, fs_int64_get(&file_info.length), mode);

    return true;
}

bool save_data_file_system_core_get_entry_type(save_data_file_system_core_ctx_t *ctx, directory_entry_type_t *out_entry_type, const char *path) {
    save_file_info_t info;
    if (save_hierarchical_file_table_try_open_file(&ctx->file_table, path, &info)) {
        *out_entry_type = DIR_ENT_TYPE_FILE;
        return true;
    }

    save_find_position_t position;
    if (save_hierarchical_file_table_try_open_directory(&ctx->file_table, path, &position)) {
        *out_entry_type = DIR_ENT_TYPE_DIR;
        return true;
    }

    return false;
}
