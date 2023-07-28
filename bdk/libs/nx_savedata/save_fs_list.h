/*
 * Copyright (c) 2019-2022 shchmue
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

#ifndef _SAVE_FS_LIST_H_
#define _SAVE_FS_LIST_H_

#include "allocation_table_storage.h"
#include "save_fs_entry.h"

#include <assert.h>
#include <stdint.h>

#define SAVE_FS_LIST_ENTRY_SIZE 0x60
#define SAVE_FS_LIST_MAX_NAME_LENGTH 0x40

typedef struct {
    uint32_t free_list_head_index;
    uint32_t used_list_head_index;
    allocation_table_storage_ctx_t storage;
} save_filesystem_list_ctx_t;

typedef struct {
    uint32_t next_sibling;
    union { /* Save table entry type. Size = 0x14. */
        save_file_info_t save_file_info;
        save_find_position_t save_find_position;
    };
} save_table_entry_t;

static_assert(sizeof(save_table_entry_t) == 0x18, "Save table entry size is wrong!");

typedef struct {
    uint32_t parent;
    char name[SAVE_FS_LIST_MAX_NAME_LENGTH];
    save_table_entry_t value;
    uint32_t next;
} save_fs_list_entry_t;

typedef struct {
    uint32_t list_size;
    uint32_t list_capacity;
    uint8_t rsvd[0x54];
    uint32_t next;
} save_fs_list_entry_meta_t;

static_assert(sizeof(save_fs_list_entry_t) == 0x60, "Save filesystem list entry size is wrong!");

static ALWAYS_INLINE uint32_t save_fs_list_read_entry(save_filesystem_list_ctx_t *ctx, uint32_t index, void *entry) {
    return save_allocation_table_storage_read(&ctx->storage, entry, index * SAVE_FS_LIST_ENTRY_SIZE, SAVE_FS_LIST_ENTRY_SIZE);
}

static ALWAYS_INLINE uint32_t save_fs_list_write_entry(save_filesystem_list_ctx_t *ctx, uint32_t index, const void *entry) {
    return save_allocation_table_storage_write(&ctx->storage, entry, index * SAVE_FS_LIST_ENTRY_SIZE, SAVE_FS_LIST_ENTRY_SIZE);
}

void save_fs_list_init(save_filesystem_list_ctx_t *ctx);
uint32_t save_fs_list_get_index_from_key(save_filesystem_list_ctx_t *ctx, const save_entry_key_t *key, uint32_t *prev_index);
bool save_fs_list_get_value_by_index(save_filesystem_list_ctx_t *ctx, uint32_t index, save_table_entry_t *value);
bool save_fs_list_get_value_and_name(save_filesystem_list_ctx_t *ctx, uint32_t index, save_table_entry_t *value, char *name);
bool save_fs_list_try_get_value_by_index(save_filesystem_list_ctx_t *ctx, uint32_t index, save_table_entry_t *value);
bool save_fs_list_try_get_value_by_key(save_filesystem_list_ctx_t *ctx, save_entry_key_t *key, save_table_entry_t *value);
bool save_fs_list_try_get_value_and_name(save_filesystem_list_ctx_t *ctx, uint32_t index, save_table_entry_t *value, char *name);
bool save_fs_list_set_value(save_filesystem_list_ctx_t *ctx, uint32_t index, const save_table_entry_t *value);
bool save_fs_list_remove(save_filesystem_list_ctx_t *ctx, const save_entry_key_t *key);
bool save_fs_list_change_key(save_filesystem_list_ctx_t *ctx, save_entry_key_t *old_key, save_entry_key_t *new_key);
uint32_t save_fs_list_add(save_filesystem_list_ctx_t *ctx, const save_entry_key_t *key, const save_table_entry_t *value);

#endif
