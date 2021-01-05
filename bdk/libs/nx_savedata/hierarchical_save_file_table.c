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

#include "hierarchical_save_file_table.h"
#include "path_parser.h"

#include <gfx_utils.h>

#include <string.h>

bool save_hierarchical_file_table_find_path_recursive(hierarchical_save_file_table_ctx_t *ctx, const char *path, save_entry_key_t *key) {
    path_parser_ctx_t parser;
    if (!save_path_parser_init(&parser, path)) {
        EPRINTF("Failed to init path parser!");
        return false;
    }

    uint32_t current_len = 0;
    const char *current = save_path_parser_get_current(&parser, &current_len);
    memset(key, 0, sizeof(save_entry_key_t));
    memcpy(key->name, current, current_len);

    while (!save_path_parser_is_finished(&parser)) {
        key->parent = save_fs_list_get_index_from_key(&ctx->directory_table, key, NULL);

        if (key->parent & 0x80000000)
            return false;

        save_path_parser_try_get_next(&parser, key->name);
    }
    return true;
}

bool save_hierarchical_file_table_try_open_file(hierarchical_save_file_table_ctx_t *ctx, const char *path, save_file_info_t *file_info) {
    save_entry_key_t key;
    if (!save_hierarchical_file_table_find_path_recursive(ctx, path, &key)) {
        memset(file_info, 0, sizeof(save_file_info_t));
        return false;
    }

    save_table_entry_t value;
    if (save_fs_list_try_get_value_by_key(&ctx->file_table, &key, &value)) {
        memcpy(file_info, &value.save_file_info, sizeof(save_file_info_t));
        return true;
    }

    memset(file_info, 0, sizeof(save_file_info_t));
    return false;
}

bool save_hierarchical_file_table_try_open_directory(hierarchical_save_file_table_ctx_t *ctx, const char *path, save_find_position_t *position) {
    save_entry_key_t key;
    if (!save_hierarchical_file_table_find_path_recursive(ctx, path, &key)) {
        memset(position, 0, sizeof(save_find_position_t));
        return false;
    }

    save_table_entry_t entry;
    if (save_fs_list_try_get_value_by_key(&ctx->file_table, &key, &entry)) {
        memcpy(position, &entry.save_find_position, sizeof(save_find_position_t));
        return true;
    }

    memset(position, 0, sizeof(save_find_position_t));
    return false;
}

bool save_hierarchical_file_table_find_next_file(hierarchical_save_file_table_ctx_t *ctx, save_find_position_t *position, save_file_info_t *info, char *name) {
    if (position->next_file == 0) {
        memset(info, 0, sizeof(save_file_info_t));
        memset(name, 0, SAVE_FS_LIST_MAX_NAME_LENGTH);
        return false;
    }

    save_table_entry_t entry;
    if (!save_fs_list_try_get_value_and_name(&ctx->file_table, position->next_file, &entry, name)) {
        memset(info, 0, sizeof(save_file_info_t));
        memset(name, 0, SAVE_FS_LIST_MAX_NAME_LENGTH);
        return false;
    }

    position->next_file = entry.next_sibling;
    memcpy(info, &entry.save_file_info, sizeof(save_file_info_t));
    return true;
}

bool save_hierarchical_file_table_find_next_directory(hierarchical_save_file_table_ctx_t *ctx, save_find_position_t *position, char *name) {
    if (position->next_directory == 0) {
        return false;
    }

    save_table_entry_t entry;
    if(!save_fs_list_try_get_value_and_name(&ctx->directory_table, position->next_directory, &entry, name)) {
        memset(name, 0, SAVE_FS_LIST_MAX_NAME_LENGTH);
        return false;
    }

    position->next_directory = entry.next_sibling;
    return true;
}

static bool save_hierarchical_file_table_link_file_to_parent(hierarchical_save_file_table_ctx_t *ctx, uint32_t parent_index, uint32_t file_index) {
    save_table_entry_t parent_entry, file_entry;
    if (!save_fs_list_get_value_by_index(&ctx->directory_table, parent_index, &parent_entry)) {
        EPRINTF("Failed to get directory table value!");
        return false;
    }
    if (!save_fs_list_get_value_by_index(&ctx->file_table, file_index, &file_entry)) {
        EPRINTF("Failed to get file table value!");
        return false;
    }

    file_entry.next_sibling = parent_entry.save_find_position.next_file;
    parent_entry.save_find_position.next_file = file_index;

    if (!save_fs_list_set_value(&ctx->directory_table, parent_index, &parent_entry)) {
        EPRINTF("Failed to set directory table value!");
        return false;
    }
    if (!save_fs_list_set_value(&ctx->file_table, file_index, &file_entry)) {
        EPRINTF("Failed to set file table value!");
        return false;
    }

    return true;
}


static bool save_hierarchical_file_table_link_directory_to_parent(hierarchical_save_file_table_ctx_t *ctx, uint32_t parent_index, uint32_t dir_index) {
    save_table_entry_t parent_entry, dir_entry;
    if (!save_fs_list_get_value_by_index(&ctx->directory_table, parent_index, &parent_entry)) {
        EPRINTF("Failed to get parent directory table value!");
        return false;
    }
    if (!save_fs_list_get_value_by_index(&ctx->directory_table, dir_index, &dir_entry)) {
        EPRINTF("Failed to get directory table value!");
        return false;
    }

    dir_entry.next_sibling = parent_entry.save_find_position.next_directory;
    parent_entry.save_find_position.next_directory = dir_index;

    if (!save_fs_list_set_value(&ctx->directory_table, parent_index, &parent_entry)) {
        EPRINTF("Failed to set parent directory table value!");
        return false;
    }
    if (!save_fs_list_set_value(&ctx->directory_table, dir_index, &dir_entry)) {
        EPRINTF("Failed to set directory table value!");
        return false;
    }

    return true;
}

void save_hierarchical_file_table_unlink_file_from_parent(hierarchical_save_file_table_ctx_t *ctx, uint32_t parent_index, uint32_t file_index) {
    save_table_entry_t parent_entry, file_entry;
    save_fs_list_get_value_by_index(&ctx->directory_table, parent_index, &parent_entry);
    save_fs_list_get_value_by_index(&ctx->file_table, file_index, &file_entry);

    if (parent_entry.save_find_position.next_file == file_index) {
        parent_entry.save_find_position.next_file = file_entry.next_sibling;
        save_fs_list_set_value(&ctx->directory_table, parent_index, &parent_entry);
        return;
    }

    uint32_t prev_index = parent_entry.save_find_position.next_file;
    save_table_entry_t prev_entry, cur_entry;
    save_fs_list_get_value_by_index(&ctx->file_table, prev_index, &prev_entry);
    uint32_t cur_index = prev_entry.next_sibling;

    while (cur_index != 0) {
        save_fs_list_get_value_by_index(&ctx->file_table, cur_index, &cur_entry);
        if (cur_index == file_index) {
            prev_entry.next_sibling = cur_entry.next_sibling;
            save_fs_list_set_value(&ctx->file_table, prev_index, &prev_entry);
            return;
        }
        prev_index = cur_index;
        memcpy(&prev_entry, &cur_entry, sizeof(save_table_entry_t));
        cur_index = prev_entry.next_sibling;
    }
}

void save_hierarchical_file_table_unlink_directory_from_parent(hierarchical_save_file_table_ctx_t *ctx, uint32_t parent_index, uint32_t dir_index) {
    save_table_entry_t parent_entry, dir_entry;
    save_fs_list_get_value_by_index(&ctx->directory_table, parent_index, &parent_entry);
    save_fs_list_get_value_by_index(&ctx->directory_table, dir_index, &dir_entry);

    if (parent_entry.save_find_position.next_directory == dir_index) {
        parent_entry.save_find_position.next_directory = dir_entry.next_sibling;
        save_fs_list_set_value(&ctx->directory_table, parent_index, &parent_entry);
        return;
    }

    uint32_t prev_index = parent_entry.save_find_position.next_directory;
    save_table_entry_t prev_entry, cur_entry;
    save_fs_list_get_value_by_index(&ctx->directory_table, prev_index, &prev_entry);
    uint32_t cur_index = prev_entry.next_sibling;

    while (cur_index != 0) {
        save_fs_list_get_value_by_index(&ctx->directory_table, cur_index, &cur_entry);
        if (cur_index == dir_index) {
            prev_entry.next_sibling = cur_entry.next_sibling;
            save_fs_list_set_value(&ctx->directory_table, prev_index, &prev_entry);
            return;
        }
        prev_index = cur_index;
        memcpy(&prev_entry, &cur_entry, sizeof(save_table_entry_t));
        cur_index = prev_entry.next_sibling;
    }
}

bool save_hierarchical_file_table_delete_file(hierarchical_save_file_table_ctx_t *ctx, const char *path) {
    save_entry_key_t key;
    save_hierarchical_file_table_find_path_recursive(ctx, path, &key);

    uint32_t parent_index = key.parent;
    uint32_t to_delete_index = save_fs_list_get_index_from_key(&ctx->file_table, &key, NULL);

    if (to_delete_index == 0xFFFFFFFF) {
        EPRINTF("File not found!");
        return false;
    }

    save_hierarchical_file_table_unlink_file_from_parent(ctx, parent_index, to_delete_index);

    return save_fs_list_remove(&ctx->file_table, &key);
}

bool save_hierarchical_file_table_delete_directory(hierarchical_save_file_table_ctx_t *ctx, const char *path) {
    save_entry_key_t key;
    save_hierarchical_file_table_find_path_recursive(ctx, path, &key);

    uint32_t parent_index = key.parent;
    uint32_t to_delete_index = save_fs_list_get_index_from_key(&ctx->directory_table, &key, NULL);
    if (to_delete_index == 0xFFFFFFFF) {
        EPRINTF("Directory not found!");
        return false;
    }

    save_table_entry_t to_delete_entry;
    save_fs_list_get_value_by_index(&ctx->directory_table, to_delete_index, &to_delete_entry);
    if (to_delete_entry.save_find_position.next_directory != 0 || to_delete_entry.save_find_position.next_file != 0) {
        EPRINTF("Directory is not empty!");
        return false;
    }

    save_hierarchical_file_table_unlink_directory_from_parent(ctx, parent_index, to_delete_index);

    return save_fs_list_remove(&ctx->directory_table, &key);
}

bool save_hierarchical_file_table_rename_file(hierarchical_save_file_table_ctx_t *ctx, const char *src_path, const char *dst_path) {
    save_file_info_t file_info;
    save_find_position_t position;
    save_entry_key_t old_key, new_key;

    if (strcmp(src_path, dst_path) == 0 || save_hierarchical_file_table_try_open_file(ctx, dst_path, &file_info) || save_hierarchical_file_table_try_open_directory(ctx, dst_path, &position)) {
        EPRINTF("Destination path already exists!");
        return false;
    }

    if (!save_hierarchical_file_table_find_path_recursive(ctx, src_path, &old_key)) {
        EPRINTF("File not found!");
        return false;
    }

    uint32_t file_index = save_fs_list_get_index_from_key(&ctx->file_table, &old_key, NULL);

    if (!save_hierarchical_file_table_find_path_recursive(ctx, dst_path, &new_key)) {
        EPRINTF("File not found!");
        return false;
    }

    if (old_key.parent != new_key.parent) {
        save_hierarchical_file_table_unlink_file_from_parent(ctx, old_key.parent, file_index);
        save_hierarchical_file_table_link_file_to_parent(ctx, new_key.parent, file_index);
    }

    return save_fs_list_change_key(&ctx->file_table, &old_key, &new_key);
}

static ALWAYS_INLINE bool save_is_sub_path(const char *path1, const char *path2) {
    /* Check if either path is subpath of the other. */
    uint64_t path1_len = strlen(path1), path2_len = strlen(path2);
    if (path1_len == 0 || path2_len == 0)
        return true;

    if (path1[path1_len - 1] == '/')
        path1_len--;
    if (path2[path2_len - 1] == '/')
        path2_len--;

    const char *short_path, *long_path;
    uint64_t short_path_len, long_path_len;
    if (path1_len < path2_len) {
        short_path = path1;
        short_path_len = path1_len;
        long_path = path2;
        long_path_len = path2_len;
    } else {
        short_path = path2;
        short_path_len = path2_len;
        long_path = path1;
        long_path_len = path1_len;
    }

    if (strncmp(short_path, long_path, short_path_len) != 0)
        return false;

    return long_path_len > short_path_len + 1 && long_path[short_path_len] == '/';
}

bool save_hierarchical_file_table_rename_directory(hierarchical_save_file_table_ctx_t *ctx, const char *src_path, const char *dst_path) {
    save_file_info_t file_info;
    save_find_position_t position;
    save_entry_key_t old_key, new_key;

    if (strcmp(src_path, dst_path) == 0 || save_hierarchical_file_table_try_open_file(ctx, dst_path, &file_info) || save_hierarchical_file_table_try_open_directory(ctx, dst_path, &position)) {
        EPRINTF("Destination path already exists!");
        return false;
    }

    if (!save_hierarchical_file_table_find_path_recursive(ctx, src_path, &old_key)) {
        EPRINTF("File not found!");
        return false;
    }

    uint32_t dir_index = save_fs_list_get_index_from_key(&ctx->file_table, &old_key, NULL);

    if (!save_hierarchical_file_table_find_path_recursive(ctx, dst_path, &new_key)) {
        EPRINTF("File not found!");
        return false;
    }

    if (save_is_sub_path(src_path, dst_path)) {
        EPRINTF("Destination is subpath of source!");
        return false;
    }

    if (old_key.parent != new_key.parent) {
        save_hierarchical_file_table_unlink_directory_from_parent(ctx, old_key.parent, dir_index);
        save_hierarchical_file_table_link_directory_to_parent(ctx, new_key.parent, dir_index);
    }

    return save_fs_list_change_key(&ctx->directory_table, &old_key, &new_key);
}

uint32_t save_hierarchical_file_table_create_parent_directory_recursive(hierarchical_save_file_table_ctx_t *ctx, path_parser_ctx_t *parser, save_entry_key_t *key) {
    uint32_t prev_index = 0;

    while (!save_path_parser_is_finished(parser)) {
        uint32_t index = save_fs_list_get_index_from_key(&ctx->directory_table, key, NULL);

        if (index == 0xFFFFFFFF) {
            save_table_entry_t new_entry;
            memset(&new_entry, 0, sizeof(new_entry));
            index = save_fs_list_add(&ctx->directory_table, key, &new_entry);

            if ((prev_index & 0x80000000) == 0)
                save_hierarchical_file_table_link_directory_to_parent(ctx, prev_index, index);
        }

        prev_index = index;
        key->parent = index;
        save_path_parser_try_get_next(parser, key->name);
    }

    return prev_index;
}

static bool save_hierarchical_file_table_create_file_recursive(hierarchical_save_file_table_ctx_t *ctx, const char *path, save_file_info_t *file_info) {
    path_parser_ctx_t parser;
    if (!save_path_parser_init(&parser, path)) {
        EPRINTF("Failed to init path parser!");
        return false;
    }

    save_entry_key_t key = {"", 0};
    uint32_t current_len = 0;
    const char *current = save_path_parser_get_current(&parser, &current_len);
    memcpy(key.name, current, current_len);

    uint32_t parent_index = save_hierarchical_file_table_create_parent_directory_recursive(ctx, &parser, &key);

    uint32_t index = save_fs_list_get_index_from_key(&ctx->file_table, &key, NULL);

    save_table_entry_t file_entry;
    memset(&file_entry, 0, sizeof(file_entry));

    if ((index & 0x80000000) == 0) {
        save_fs_list_get_value_by_index(&ctx->file_table, index, &file_entry);
        memcpy(&file_entry.save_file_info, file_info, sizeof(save_file_info_t));
        save_fs_list_set_value(&ctx->file_table, index, &file_entry);
        return true;
    }

    memcpy(&file_entry.save_file_info, file_info, sizeof(save_file_info_t));
    index = save_fs_list_add(&ctx->file_table, &key, &file_entry);
    if (index == 0) {
        EPRINTF("Failed to add file to FS list!");
        return false;
    }

    return save_hierarchical_file_table_link_file_to_parent(ctx, parent_index, index);
}

static bool save_hierarchical_file_table_create_directory_recursive(hierarchical_save_file_table_ctx_t *ctx, const char *path) {
    path_parser_ctx_t parser;
    if (!save_path_parser_init(&parser, path)) {
        EPRINTF("Failed to init path parser!");
        return false;
    }

    save_entry_key_t key = {"", 0};
    uint32_t current_len = 0;
    const char *current = save_path_parser_get_current(&parser, &current_len);
    memcpy(key.name, current, current_len);

    uint32_t parent_index = save_hierarchical_file_table_create_parent_directory_recursive(ctx, &parser, &key);

    uint32_t index = save_fs_list_get_index_from_key(&ctx->directory_table, &key, NULL);

    if (index != 0xFFFFFFFF)
        return true;

    save_table_entry_t dir_entry;
    memset(&dir_entry, 0, sizeof(dir_entry));
    save_fs_list_add(&ctx->directory_table, &key, &dir_entry);

    return save_hierarchical_file_table_link_directory_to_parent(ctx, parent_index, index);
}

bool save_hierarchical_file_table_add_file(hierarchical_save_file_table_ctx_t *ctx, const char *path, save_file_info_t *file_info) {
    if (strlen(path) == 1 && path[0] == '/') {
        EPRINTF("Path cannot be empty!");
        return false;
    }

    return save_hierarchical_file_table_create_file_recursive(ctx, path, file_info);
}

bool save_hierarchical_file_table_add_directory(hierarchical_save_file_table_ctx_t *ctx, const char *path) {
    if (strlen(path) == 1 && path[0] == '/') {
        EPRINTF("Directory path cannot be empty!");
        return false;
    }

    save_hierarchical_file_table_create_directory_recursive(ctx, path);
    return true;
}

