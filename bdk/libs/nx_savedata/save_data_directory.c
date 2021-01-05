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

#include "save_data_directory.h"

#include <string.h>

void save_data_directory_init(save_data_directory_ctx_t *ctx, hierarchical_save_file_table_ctx_t *table, save_find_position_t *position, open_directory_mode_t mode) {
    ctx->parent_file_table = table;
    ctx->initial_position = position;
    ctx->_current_position = position;
    ctx->mode = mode;
}

bool save_data_directory_read_impl(save_data_directory_ctx_t *ctx, uint64_t *out_entries_read, save_find_position_t *position, directory_entry_t *entry_buffer, uint64_t entry_count) {
    hierarchical_save_file_table_ctx_t *tab = ctx->parent_file_table;
    uint32_t i = 0;
    save_file_info_t info;
    char name[SAVE_FS_LIST_MAX_NAME_LENGTH];

    if (ctx->mode & OPEN_DIR_MODE_DIR) {
        while ((!entry_buffer || i < entry_count) && save_hierarchical_file_table_find_next_directory(tab, position, name)) {
            directory_entry_t *entry = &entry_buffer[i];

            memcpy(entry->name, name, SAVE_FS_LIST_MAX_NAME_LENGTH);
            entry->name[SAVE_FS_LIST_MAX_NAME_LENGTH] = 0;
            entry->type = DIR_ENT_TYPE_DIR;
            entry->size = 0;

            i++;
        }
    }

    if (ctx->mode & OPEN_DIR_MODE_FILE) {
        while ((!entry_buffer || i < entry_count) && save_hierarchical_file_table_find_next_file(tab, position, &info, name)) {
            directory_entry_t *entry = &entry_buffer[i];

            memcpy(entry->name, name, SAVE_FS_LIST_MAX_NAME_LENGTH);
            entry->name[SAVE_FS_LIST_MAX_NAME_LENGTH] = 0;
            entry->type = DIR_ENT_TYPE_FILE;
            entry->size = 0;

            i++;
        }
    }

    *out_entries_read = i;

    return true;
}

bool save_data_directory_read(save_data_directory_ctx_t *ctx, uint64_t *out_entries_read, directory_entry_t *entry_buffer, uint64_t entry_count) {
    return save_data_directory_read_impl(ctx, out_entries_read, ctx->_current_position, entry_buffer, entry_count);
}

bool save_data_directory_get_entry_count(save_data_directory_ctx_t *ctx, uint64_t *out_entry_count) {
    return save_data_directory_read_impl(ctx, out_entry_count, ctx->initial_position, NULL, 0);
}
