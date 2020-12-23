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

#ifndef _SAVE_H_
#define _SAVE_H_

#include "header.h"
#include "hierarchical_duplex_storage.h"
#include "hierarchical_integrity_verification_storage.h"
#include "journal_map.h"
#include "journal_storage.h"
#include "remap_storage.h"
#include "save_data_file_system_core.h"
#include "storage.h"

#include <libs/fatfs/ff.h>
#include <utils/types.h>

#include <assert.h>
#include <stdint.h>

#define ACTION_VERIFY (1<<2)

typedef struct {
    save_header_t header;
    FIL *file;
    uint32_t action;
    validity_t header_cmac_validity;
    validity_t header_hash_validity;
    uint8_t *data_ivfc_master;
    uint8_t *fat_ivfc_master;
    remap_storage_ctx_t data_remap_storage;
    remap_storage_ctx_t meta_remap_storage;
    hierarchical_duplex_storage_ctx_t duplex_storage;
    journal_storage_ctx_t journal_storage;
    hierarchical_integrity_verification_storage_ctx_t core_data_ivfc_storage;
    hierarchical_integrity_verification_storage_ctx_t fat_ivfc_storage;
    uint8_t *fat_storage;
    save_data_file_system_core_ctx_t save_filesystem_core;
    substorage base_storage;
    uint8_t save_mac_key[0x10];
} save_ctx_t;

typedef enum {
    SPACE_ID_SYSTEM         = 0,
    SPACE_ID_USER           = 1,
    SPACE_ID_SD_SYSTEM      = 2,
    SPACE_ID_TEMP           = 3,
    SPACE_ID_SD_USER        = 4,
    SPACE_ID_PROPER_SYSTEM  = 100,
    SPACE_ID_SAFE_MODE      = 101,
} save_data_space_id_t;

typedef struct {
    int64_t save_data_size;
    int64_t journal_size;
    int64_t block_size;
    uint64_t owner_id;
    uint32_t flags;
    uint8_t space_id;
    uint8_t pseudo;
    uint8_t reserved[0x1A];
} save_data_creation_info_t;

static_assert(sizeof(save_data_creation_info_t) == 0x40, "Save data creation info size is wrong!");

static ALWAYS_INLINE uint32_t save_calc_map_entry_storage_size(int32_t entry_count) {
    int32_t val = entry_count < 1 ? entry_count : entry_count - 1;
    return (entry_count + (val >> 1)) * sizeof(remap_entry_t);
}

void save_init(save_ctx_t *ctx, FIL *file, const uint8_t *save_mac_key, uint32_t action);
bool save_process(save_ctx_t *ctx);
bool save_create(save_ctx_t *ctx, uint32_t version, const char *mount_path, const save_data_attribute_t *attr, const save_data_creation_info_t *creation_info);
bool save_create_system_save_data(save_ctx_t *ctx, uint32_t version, const char *mount_path, uint8_t space_id, uint64_t save_id, const account_user_id_t *user_id, uint64_t owner_id, uint64_t save_data_size, uint64_t journal_size, uint32_t flags);
void save_free_contexts(save_ctx_t *ctx);
bool save_commit(save_ctx_t *ctx);

static ALWAYS_INLINE bool save_create_directory(save_ctx_t *ctx, const char *path) {
    return save_data_file_system_core_create_directory(&ctx->save_filesystem_core, path);
}

static ALWAYS_INLINE bool save_create_file(save_ctx_t *ctx, const char *path, uint64_t size) {
    return save_data_file_system_core_create_file(&ctx->save_filesystem_core, path, size);
}

static ALWAYS_INLINE bool save_delete_directory(save_ctx_t *ctx, const char *path) {
    return save_data_file_system_core_delete_directory(&ctx->save_filesystem_core,path);
}

static ALWAYS_INLINE bool save_delete_file(save_ctx_t *ctx, const char *path) {
    return save_data_file_system_core_delete_file(&ctx->save_filesystem_core, path);
}

static ALWAYS_INLINE bool save_open_directory(save_ctx_t *ctx, save_data_directory_ctx_t *directory, const char *path, open_directory_mode_t mode) {
    return save_data_file_system_core_open_directory(&ctx->save_filesystem_core, directory, path, mode);
}

static ALWAYS_INLINE bool save_open_file(save_ctx_t *ctx, save_data_file_ctx_t *file, const char *path, open_mode_t mode) {
    return save_data_file_system_core_open_file(&ctx->save_filesystem_core, file, path, mode);
}

static ALWAYS_INLINE bool save_rename_directory(save_ctx_t *ctx, const char *old_path, const char *new_path) {
    return save_data_file_system_core_rename_directory(&ctx->save_filesystem_core, old_path, new_path);
}

static ALWAYS_INLINE bool save_rename_file(save_ctx_t *ctx, const char *old_path, const char *new_path) {
    return save_data_file_system_core_rename_file(&ctx->save_filesystem_core, old_path, new_path);
}

static ALWAYS_INLINE bool save_get_entry_type(save_ctx_t *ctx, directory_entry_type_t *out_entry_type, const char *path) {
    return save_data_file_system_core_get_entry_type(&ctx->save_filesystem_core, out_entry_type, path);
}

static ALWAYS_INLINE void save_get_free_space_size(save_ctx_t *ctx, uint64_t *out_free_space) {
    return save_data_file_system_core_get_free_space_size(&ctx->save_filesystem_core, out_free_space);
}

static ALWAYS_INLINE void save_get_total_space_size(save_ctx_t *ctx, uint64_t *out_total_size) {
    return save_data_file_system_core_get_total_space_size(&ctx->save_filesystem_core, out_total_size);
}

#endif
