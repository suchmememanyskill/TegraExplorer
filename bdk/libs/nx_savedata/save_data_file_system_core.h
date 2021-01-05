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

#ifndef _SAVE_DATA_FILE_SYSTEM_CORE_H_
#define _SAVE_DATA_FILE_SYSTEM_CORE_H_

#include "allocation_table.h"
#include "header.h"
#include "hierarchical_save_file_table.h"
#include "save_data_directory.h"
#include "save_data_file.h"
#include "storage.h"

typedef struct {
    substorage *base_storage;
    allocation_table_ctx_t allocation_table;
    save_fs_header_t *header;
    hierarchical_save_file_table_ctx_t file_table;
} save_data_file_system_core_ctx_t;

static ALWAYS_INLINE bool save_data_file_system_core_rename_directory(save_data_file_system_core_ctx_t *ctx, const char *old_path, const char *new_path) {
    return save_hierarchical_file_table_rename_directory(&ctx->file_table, old_path, new_path);
}

static ALWAYS_INLINE bool save_data_file_system_core_rename_file(save_data_file_system_core_ctx_t *ctx, const char *old_path, const char *new_path) {
    return save_hierarchical_file_table_rename_file(&ctx->file_table, old_path, new_path);
}

static ALWAYS_INLINE void save_data_file_system_core_get_free_space_size(save_data_file_system_core_ctx_t *ctx, uint64_t *out_free_space) {
    uint32_t free_block_count = save_allocation_table_get_free_list_length(&ctx->allocation_table);
    *out_free_space = ctx->header->block_size * free_block_count;
}

static ALWAYS_INLINE void save_data_file_system_core_get_total_space_size(save_data_file_system_core_ctx_t *ctx, uint64_t *out_total_space) {
    *out_total_space = ctx->header->block_size * ctx->header->block_count;
}

bool save_data_file_system_core_init(save_data_file_system_core_ctx_t *ctx, substorage *storage, void *allocation_table, save_fs_header_t *save_fs_header);

bool save_data_file_system_core_create_directory(save_data_file_system_core_ctx_t *ctx, const char *path);
bool save_data_file_system_core_create_file(save_data_file_system_core_ctx_t *ctx, const char *path, uint64_t size);
bool save_data_file_system_core_delete_directory(save_data_file_system_core_ctx_t *ctx, const char *path);
bool save_data_file_system_core_delete_file(save_data_file_system_core_ctx_t *ctx, const char *path);
bool save_data_file_system_core_open_directory(save_data_file_system_core_ctx_t *ctx, save_data_directory_ctx_t *directory, const char *path, open_directory_mode_t mode);
bool save_data_file_system_core_open_file(save_data_file_system_core_ctx_t *ctx, save_data_file_ctx_t *file, const char *path, open_mode_t mode);
bool save_data_file_system_core_get_entry_type(save_data_file_system_core_ctx_t *ctx, directory_entry_type_t *out_entry_type, const char *path);

#endif
