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

#include "save_fs_list.h"

#include <gfx_utils.h>

#include <string.h>

void save_fs_list_init(save_filesystem_list_ctx_t *ctx) {
    ctx->free_list_head_index = 0;
    ctx->used_list_head_index = 1;
}

static ALWAYS_INLINE uint32_t save_fs_list_get_capacity(save_filesystem_list_ctx_t *ctx) {
    uint32_t capacity;
    if (save_allocation_table_storage_read(&ctx->storage, &capacity, 4, 4) != 4) {
        EPRINTF("Failed to read FS list capacity!");
        return 0;
    }
    return capacity;
}

static ALWAYS_INLINE uint32_t save_fs_list_get_length(save_filesystem_list_ctx_t *ctx) {
    uint32_t length;
    if (save_allocation_table_storage_read(&ctx->storage, &length, 0, 4) != 4) {
        EPRINTF("Failed to read FS list length!");
        return 0;
    }
    return length;
}

static ALWAYS_INLINE bool save_fs_list_set_capacity(save_filesystem_list_ctx_t *ctx, uint32_t capacity) {
    return save_allocation_table_storage_write(&ctx->storage, &capacity, 4, 4) == 4;
}

static ALWAYS_INLINE bool save_fs_list_set_length(save_filesystem_list_ctx_t *ctx, uint32_t length) {
    return save_allocation_table_storage_write(&ctx->storage, &length, 0, 4) == 4;
}

uint32_t save_fs_list_get_index_from_key(save_filesystem_list_ctx_t *ctx, const save_entry_key_t *key, uint32_t *prev_index) {
    save_fs_list_entry_t entry;
    uint32_t capacity = save_fs_list_get_capacity(ctx);
    if (save_fs_list_read_entry(ctx, ctx->used_list_head_index, &entry) != SAVE_FS_LIST_ENTRY_SIZE) {
        EPRINTF("Failed to read used list head entry!");
        return 0xFFFFFFFF;
    }
    uint32_t prev;
    if (!prev_index) {
        prev_index = &prev;
    }
    *prev_index = ctx->used_list_head_index;
    uint32_t index = entry.next;
    while (index) {
        if (index > capacity) {
            EPRINTFARGS("Save entry index %d out of range!", index);
            return 0xFFFFFFFF;
        }
        if (save_fs_list_read_entry(ctx, index, &entry) != SAVE_FS_LIST_ENTRY_SIZE)
            return 0xFFFFFFFF;
        if (entry.parent == key->parent && !strcmp(entry.name, key->name)) {
            return index;
        }
        *prev_index = index;
        index = entry.next;
    }
    *prev_index = 0xFFFFFFFF;
    return 0xFFFFFFFF;
}

bool save_fs_list_get_value_by_index(save_filesystem_list_ctx_t *ctx, uint32_t index, save_table_entry_t *value) {
    save_fs_list_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    if (save_fs_list_read_entry(ctx, index, &entry) != SAVE_FS_LIST_ENTRY_SIZE) {
        EPRINTFARGS("Failed to read FS list entry at index %x!", index);
        return false;
    }
    memcpy(value, &entry.value, sizeof(save_table_entry_t));
    return true;
}

bool save_fs_list_get_value_and_name(save_filesystem_list_ctx_t *ctx, uint32_t index, save_table_entry_t *value, char *name) {
    save_fs_list_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    if (save_fs_list_read_entry(ctx, index, &entry) != SAVE_FS_LIST_ENTRY_SIZE) {
        EPRINTFARGS("Failed to read FS list entry at index %x!", index);
        return false;
    }
    memcpy(value, &entry.value, sizeof(save_table_entry_t));
    memcpy(name, entry.name, SAVE_FS_LIST_MAX_NAME_LENGTH);
    return true;
}

bool save_fs_list_try_get_value_by_index(save_filesystem_list_ctx_t *ctx, uint32_t index, save_table_entry_t *value) {
    if ((index & 0x80000000) != 0 || index >= save_fs_list_get_capacity(ctx)) {
        memset(value, 0, sizeof(save_table_entry_t));
        return false;
    }

    return save_fs_list_get_value_by_index(ctx, index, value);
}

bool save_fs_list_try_get_value_by_key(save_filesystem_list_ctx_t *ctx, save_entry_key_t *key, save_table_entry_t *value) {
    uint32_t index = save_fs_list_get_index_from_key(ctx, key, NULL);

    if ((index & 0x80000000) != 0) {
        memset(value, 0, sizeof(save_table_entry_t));
        return false;
    }

    return save_fs_list_try_get_value_by_index(ctx, index, value);
}

bool save_fs_list_try_get_value_and_name(save_filesystem_list_ctx_t *ctx, uint32_t index, save_table_entry_t *value, char *name) {
    if ((index & 0x80000000) != 0 || index >= save_fs_list_get_capacity(ctx)) {
        memset(value, 0, sizeof(save_table_entry_t));
        return false;
    }

    return save_fs_list_get_value_and_name(ctx, index, value, name);
}

bool save_fs_list_set_value(save_filesystem_list_ctx_t *ctx, uint32_t index, const save_table_entry_t *value) {
    save_fs_list_entry_t entry = {0};
    if (save_fs_list_read_entry(ctx, index, &entry) != SAVE_FS_LIST_ENTRY_SIZE)
        return false;
    memcpy(&entry.value, value, sizeof(save_table_entry_t));
    return save_fs_list_write_entry(ctx, index, &entry) == SAVE_FS_LIST_ENTRY_SIZE;
}

bool save_fs_list_free(save_filesystem_list_ctx_t *ctx, uint32_t entry_index) {
    save_fs_list_entry_t free_entry, entry;
    if (save_fs_list_read_entry(ctx, ctx->free_list_head_index, &free_entry) != SAVE_FS_LIST_ENTRY_SIZE)
        return false;
    if (save_fs_list_read_entry(ctx, entry_index, &entry) != SAVE_FS_LIST_ENTRY_SIZE)
        return false;

    entry.next = free_entry.next;
    free_entry.next = entry_index;

    if (save_fs_list_write_entry(ctx, ctx->free_list_head_index, &free_entry) != SAVE_FS_LIST_ENTRY_SIZE)
        return false;
    return save_fs_list_write_entry(ctx, entry_index, &entry) == SAVE_FS_LIST_ENTRY_SIZE;
}

bool save_fs_list_remove(save_filesystem_list_ctx_t *ctx, const save_entry_key_t *key) {
    uint32_t index, previous_index;
    index = save_fs_list_get_index_from_key(ctx, key, &previous_index);
    if (index == 0xFFFFFFFF)
        return false;

    save_fs_list_entry_t prev_entry, entry_to_del;
    if (save_fs_list_read_entry(ctx, previous_index, &prev_entry) != SAVE_FS_LIST_ENTRY_SIZE)
        return false;
    if (save_fs_list_read_entry(ctx, index, &entry_to_del) != SAVE_FS_LIST_ENTRY_SIZE)
        return false;

    prev_entry.next = entry_to_del.next;
    if (save_fs_list_write_entry(ctx, previous_index, &prev_entry) != SAVE_FS_LIST_ENTRY_SIZE)
        return false;

    return save_fs_list_free(ctx, index);
}

bool save_fs_list_change_key(save_filesystem_list_ctx_t *ctx, save_entry_key_t *old_key, save_entry_key_t *new_key) {
    uint32_t index = save_fs_list_get_index_from_key(ctx, old_key, NULL);
    uint32_t new_index = save_fs_list_get_index_from_key(ctx, new_key, NULL);

    if (index == 0xFFFFFFFF) {
        EPRINTF("Old key was not found!");
        return false;
    }
    if (new_index != 0xFFFFFFFF) {
        EPRINTF("New key already exists!");
        return false;
    }

    save_fs_list_entry_t entry;
    if (save_fs_list_read_entry(ctx, index, &entry) != SAVE_FS_LIST_ENTRY_SIZE)
        return false;

    entry.parent = new_key->parent;
    memcpy(entry.name, new_key->name, SAVE_FS_LIST_MAX_NAME_LENGTH);

    return save_fs_list_write_entry(ctx, index, &entry) == SAVE_FS_LIST_ENTRY_SIZE;
}

uint32_t save_fs_list_allocate_entry(save_filesystem_list_ctx_t *ctx) {
    save_fs_list_entry_t free_list_head, used_list_head;
    if (save_fs_list_read_entry(ctx, ctx->free_list_head_index, &free_list_head) != SAVE_FS_LIST_ENTRY_SIZE)
        return 0;
    if (save_fs_list_read_entry(ctx, ctx->used_list_head_index, &used_list_head) != SAVE_FS_LIST_ENTRY_SIZE)
        return 0;

    uint32_t allocated_index = free_list_head.next;

    if (allocated_index != 0) {
        save_fs_list_entry_t first_free_entry;
        if (save_fs_list_read_entry(ctx, allocated_index, &first_free_entry) != SAVE_FS_LIST_ENTRY_SIZE)
            return 0;

        free_list_head.next = first_free_entry.next;
        first_free_entry.next = used_list_head.next;
        used_list_head.next = allocated_index;

        if (save_fs_list_write_entry(ctx, ctx->free_list_head_index, &free_list_head) != SAVE_FS_LIST_ENTRY_SIZE)
            return 0;
        if (save_fs_list_write_entry(ctx, ctx->used_list_head_index, &used_list_head) != SAVE_FS_LIST_ENTRY_SIZE)
            return 0;
        if (save_fs_list_write_entry(ctx, allocated_index, &first_free_entry) != SAVE_FS_LIST_ENTRY_SIZE)
            return 0;

        return allocated_index;
    }

    uint32_t length = save_fs_list_get_length(ctx);
    uint32_t capacity = save_fs_list_get_capacity(ctx);

    if (capacity == 0 || length >= capacity) {
        uint64_t current_size, new_size;
        save_allocation_table_storage_get_size(&ctx->storage, &current_size);
        if (!save_allocation_table_storage_set_size(&ctx->storage, current_size + SZ_16K))
            return 0;
        save_allocation_table_storage_get_size(&ctx->storage, &new_size);
        if (!save_fs_list_set_capacity(ctx, (uint32_t)(new_size / sizeof(save_fs_list_entry_t))))
            return 0;
    }

    if (!save_fs_list_set_length(ctx, length + 1))
        return 0;

    save_fs_list_entry_t new_entry;
    if (save_fs_list_read_entry(ctx, length, &new_entry) != SAVE_FS_LIST_ENTRY_SIZE)
        return 0;

    new_entry.next = used_list_head.next;
    used_list_head.next = length;

    if (save_fs_list_write_entry(ctx, ctx->used_list_head_index, &used_list_head) != SAVE_FS_LIST_ENTRY_SIZE)
        return 0;
    if (save_fs_list_write_entry(ctx, length, &new_entry) != SAVE_FS_LIST_ENTRY_SIZE)
        return 0;

    return length;
}

uint32_t save_fs_list_add(save_filesystem_list_ctx_t *ctx, const save_entry_key_t *key, const save_table_entry_t *value) {
    uint32_t index = save_fs_list_get_index_from_key(ctx, key, NULL);

    if (index != 0xFFFFFFFF) {
        save_fs_list_set_value(ctx, index, value);
        return index;
    }

    index = save_fs_list_allocate_entry(ctx);
    if (index == 0) {
        EPRINTF("Failed to allocate FS list entry!");
        return 0;
    }

    save_fs_list_entry_t entry;
    save_fs_list_read_entry(ctx, index, &entry);
    memcpy(&entry.value, value, sizeof(save_table_entry_t));
    entry.parent = key->parent;
    memcpy(entry.name, key->name, SAVE_FS_LIST_MAX_NAME_LENGTH);
    save_fs_list_write_entry(ctx, index, &entry);

    return index;
}
