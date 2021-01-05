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

#include "save.h"

#include <gfx_utils.h>
#include <mem/heap.h>
#include <rtc/max77620-rtc.h>
#include <sec/se.h>
#include <storage/nx_sd.h>
#include <utils/ini.h>
#include <utils/sprintf.h>

#include <stdlib.h>
#include <string.h>

static void save_init_journal_ivfc_storage(save_ctx_t *ctx, hierarchical_integrity_verification_storage_ctx_t *out_ivfc, int integrity_check_level) {
    const uint32_t ivfc_levels = 5;
    ivfc_save_hdr_t *ivfc = &ctx->header.data_ivfc_header;
    substorage levels[ivfc_levels];

    substorage_init(&levels[0], &memory_storage_vt, ctx->data_ivfc_master, 0, ctx->header.layout.ivfc_master_hash_size);
    for (unsigned int i = 0; i < ivfc_levels - 2; i++) {
        ivfc_level_hdr_t *level = &ivfc->level_hash_info.level_headers[i];
        substorage_init(&levels[i + 1], &remap_storage_vt, &ctx->meta_remap_storage, fs_int64_get(&level->logical_offset), fs_int64_get(&level->hash_data_size));
    }
    ivfc_level_hdr_t *data_level = &ivfc->level_hash_info.level_headers[ivfc_levels - 2];
    substorage_init(&levels[ivfc_levels - 1], &journal_storage_vt, &ctx->journal_storage, fs_int64_get(&data_level->logical_offset), fs_int64_get(&data_level->hash_data_size));

    save_hierarchical_integrity_verification_storage_init_with_levels(out_ivfc, ivfc, ivfc_levels, levels, integrity_check_level);
}

static void save_init_fat_ivfc_storage(save_ctx_t *ctx, hierarchical_integrity_verification_storage_ctx_t *out_ivfc, int integrity_check_level) {
    substorage fat_ivfc_master;
    substorage_init(&fat_ivfc_master, &memory_storage_vt, ctx->fat_ivfc_master, 0, ctx->header.layout.ivfc_master_hash_size);
    save_hierarchical_integrity_verification_storage_init_for_fat(out_ivfc, &ctx->header.version_5.fat_ivfc_header, &fat_ivfc_master, &ctx->meta_remap_storage.base_storage, integrity_check_level);
}

static validity_t save_filesystem_verify(save_ctx_t *ctx) {
    validity_t journal_validity = save_hierarchical_integrity_verification_storage_validate(&ctx->core_data_ivfc_storage);
    save_hierarchical_integrity_verification_storage_set_level_validities(&ctx->core_data_ivfc_storage);

    if (ctx->header.layout.version < VERSION_DISF_5)
        return journal_validity;

    validity_t fat_validity = save_hierarchical_integrity_verification_storage_validate(&ctx->fat_ivfc_storage);
    save_hierarchical_integrity_verification_storage_set_level_validities(&ctx->core_data_ivfc_storage);

    if (journal_validity != VALIDITY_VALID)
        return journal_validity;
    if (fat_validity != VALIDITY_VALID)
        return fat_validity;

    return journal_validity;
}

static bool save_process_header(save_ctx_t *ctx) {
    if (ctx->header.layout.magic != MAGIC_DISF || ctx->header.duplex_header.magic != MAGIC_DPFS ||
        ctx->header.data_ivfc_header.magic != MAGIC_IVFC || ctx->header.journal_header.magic != MAGIC_JNGL ||
        ctx->header.save_header.magic != MAGIC_SAVE || ctx->header.main_remap_header.magic != MAGIC_RMAP ||
        ctx->header.meta_remap_header.magic != MAGIC_RMAP)
    {
        EPRINTF("Error: Save header is corrupt!");
        return false;
    }

    ctx->data_ivfc_master = (uint8_t *)&ctx->header + ctx->header.layout.ivfc_master_hash_offset_a;
    ctx->fat_ivfc_master = (uint8_t *)&ctx->header + ctx->header.layout.fat_ivfc_master_hash_a;

    uint8_t hash[0x20] __attribute__((aligned(4)));
    uint32_t hashed_data_offset = sizeof(ctx->header.layout) + sizeof(ctx->header.cmac) + sizeof(ctx->header._0x10);
    uint32_t hashed_data_size = sizeof(ctx->header) - hashed_data_offset;
    se_calc_sha256_oneshot(hash, (uint8_t *)&ctx->header + hashed_data_offset, hashed_data_size);
    ctx->header_hash_validity = memcmp(hash, ctx->header.layout.hash, sizeof(hash)) == 0 ? VALIDITY_VALID : VALIDITY_INVALID;

    uint8_t cmac[0x10] __attribute__((aligned(4)));
    se_aes_key_set(10, ctx->save_mac_key, 0x10);
    se_aes_cmac(10, cmac, 0x10, &ctx->header.layout, sizeof(ctx->header.layout));
    if (memcmp(cmac, &ctx->header.cmac, 0x10) == 0) {
        ctx->header_cmac_validity = VALIDITY_VALID;
    } else {
        ctx->header_cmac_validity = VALIDITY_INVALID;
    }

    return true;
}

void save_init(save_ctx_t *ctx, FIL *file, const uint8_t *save_mac_key, uint32_t action) {
    ctx->file = file;
    ctx->action = action;
    memcpy(ctx->save_mac_key, save_mac_key, sizeof(ctx->save_mac_key));
}

bool save_process(save_ctx_t *ctx) {
    substorage_init(&ctx->base_storage, &file_storage_vt, ctx->file, 0, f_size(ctx->file));
    /* Try to parse Header A. */
    if (substorage_read(&ctx->base_storage, &ctx->header, 0, sizeof(ctx->header)) != sizeof(ctx->header)) {
        EPRINTF("Failed to read save header A!\n");
        return false;
    }

    if (!save_process_header(ctx) || (ctx->header_hash_validity == VALIDITY_INVALID)) {
        /* Try to parse Header B. */
        if (substorage_read(&ctx->base_storage, &ctx->header, sizeof(ctx->header), sizeof(ctx->header)) != sizeof(ctx->header)) {
            EPRINTF("Failed to read save header B!\n");
            return false;
        }

        if (!save_process_header(ctx) || (ctx->header_hash_validity == VALIDITY_INVALID)) {
            EPRINTF("Error: Save header is invalid!");
            return false;
        }
    }

    if (ctx->header.layout.version > VERSION_DISF_5) {
        EPRINTF("Unsupported save version.\nLibrary must be updated.");
        return false;
    }

    /* Initialize remap storages. */
    ctx->data_remap_storage.header = &ctx->header.main_remap_header;
    ctx->meta_remap_storage.header = &ctx->header.meta_remap_header;

    u32 data_remap_entry_size = sizeof(remap_entry_t) * ctx->data_remap_storage.header->map_entry_count;
    u32 meta_remap_entry_size = sizeof(remap_entry_t) * ctx->meta_remap_storage.header->map_entry_count;

    substorage_init(&ctx->data_remap_storage.base_storage, &file_storage_vt, ctx->file, ctx->header.layout.file_map_data_offset, ctx->header.layout.file_map_data_size);
    ctx->data_remap_storage.map_entries = calloc(1, sizeof(remap_entry_ctx_t) * ctx->data_remap_storage.header->map_entry_count);
    uint8_t *remap_buffer = malloc(MAX(data_remap_entry_size, meta_remap_entry_size));
    if (substorage_read(&ctx->base_storage, remap_buffer, ctx->header.layout.file_map_entry_offset, data_remap_entry_size) != data_remap_entry_size) {
        EPRINTF("Failed to read data remap table!");
        free(remap_buffer);
        return false;
    }
    for (unsigned int i = 0; i < ctx->data_remap_storage.header->map_entry_count; i++) {
        memcpy(&ctx->data_remap_storage.map_entries[i], remap_buffer + sizeof(remap_entry_t) * i, sizeof(remap_entry_t));
        ctx->data_remap_storage.map_entries[i].ends.physical_offset_end = ctx->data_remap_storage.map_entries[i].entry.physical_offset + ctx->data_remap_storage.map_entries[i].entry.size;
        ctx->data_remap_storage.map_entries[i].ends.virtual_offset_end = ctx->data_remap_storage.map_entries[i].entry.virtual_offset + ctx->data_remap_storage.map_entries[i].entry.size;
    }

    /* Initialize data remap storage. */
    ctx->data_remap_storage.segments = save_remap_storage_init_segments(&ctx->data_remap_storage);
    if (!ctx->data_remap_storage.segments) {
        free(remap_buffer);
        return false;
    }

    /* Initialize hierarchical duplex storage. */
    if (!save_hierarchical_duplex_storage_init(&ctx->duplex_storage, &ctx->data_remap_storage, &ctx->header)) {
        free(remap_buffer);
        return false;
    }

    /* Initialize meta remap storage. */
    substorage_init(&ctx->meta_remap_storage.base_storage, &hierarchical_duplex_storage_vt, &ctx->duplex_storage, 0, ctx->duplex_storage.data_layer->_length);
    ctx->meta_remap_storage.map_entries = calloc(1, sizeof(remap_entry_ctx_t) * ctx->meta_remap_storage.header->map_entry_count);
    if (substorage_read(&ctx->base_storage, remap_buffer, ctx->header.layout.meta_map_entry_offset, meta_remap_entry_size) != meta_remap_entry_size) {
        EPRINTF("Failed to read meta remap table!");
        free(remap_buffer);
        return false;
    }
    for (unsigned int i = 0; i < ctx->meta_remap_storage.header->map_entry_count; i++) {
        memcpy(&ctx->meta_remap_storage.map_entries[i], remap_buffer + sizeof(remap_entry_t) * i, sizeof(remap_entry_t));
        ctx->meta_remap_storage.map_entries[i].ends.physical_offset_end = ctx->meta_remap_storage.map_entries[i].entry.physical_offset + ctx->meta_remap_storage.map_entries[i].entry.size;
        ctx->meta_remap_storage.map_entries[i].ends.virtual_offset_end = ctx->meta_remap_storage.map_entries[i].entry.virtual_offset + ctx->meta_remap_storage.map_entries[i].entry.size;
    }
    free(remap_buffer);

    ctx->meta_remap_storage.segments = save_remap_storage_init_segments(&ctx->meta_remap_storage);
    if (!ctx->meta_remap_storage.segments)
        return false;

    /* Initialize journal map. */
    journal_map_params_t journal_map_info;
    journal_map_info.map_storage = malloc(ctx->header.layout.journal_map_table_size);
    if (save_remap_storage_read(&ctx->meta_remap_storage, journal_map_info.map_storage, ctx->header.layout.journal_map_table_offset, ctx->header.layout.journal_map_table_size) != ctx->header.layout.journal_map_table_size) {
        EPRINTF("Failed to read journal map!");
        return false;
    }

    /* Initialize journal storage. */
    substorage journal_data;
    substorage_init(&journal_data, &remap_storage_vt, &ctx->data_remap_storage, ctx->header.layout.journal_data_offset, ctx->header.layout.journal_data_size_b + ctx->header.layout.journal_size);

    save_journal_storage_init(&ctx->journal_storage, &journal_data, &ctx->header.journal_header, &journal_map_info);

    /* Initialize core IVFC storage. */
    save_init_journal_ivfc_storage(ctx, &ctx->core_data_ivfc_storage, ctx->action & ACTION_VERIFY);

    /* Initialize FAT storage. */
    if (ctx->header.layout.version < VERSION_DISF_5) {
        ctx->fat_storage = malloc(ctx->header.layout.fat_size);
        save_remap_storage_read(&ctx->meta_remap_storage, ctx->fat_storage, ctx->header.layout.fat_offset, ctx->header.layout.fat_size);
    } else {
        save_init_fat_ivfc_storage(ctx, &ctx->fat_ivfc_storage, ctx->action & ACTION_VERIFY);
        ctx->fat_storage = malloc(ctx->fat_ivfc_storage.length);
        save_remap_storage_read(&ctx->meta_remap_storage, ctx->fat_storage, fs_int64_get(&ctx->header.version_5.fat_ivfc_header.level_hash_info.level_headers[2].logical_offset), ctx->fat_ivfc_storage.length);
    }

    if (ctx->action & ACTION_VERIFY) {
        save_filesystem_verify(ctx);
    }

    /* Initialize core save filesystem. */
    return save_data_file_system_core_init(&ctx->save_filesystem_core, &ctx->core_data_ivfc_storage.base_storage, ctx->fat_storage, &ctx->header.save_header);
}

void save_free_contexts(save_ctx_t *ctx) {
    if (ctx->data_remap_storage.header) {
        for (unsigned int i = 0; i < ctx->data_remap_storage.header->map_segment_count; i++) {
            if (ctx->data_remap_storage.segments && ctx->data_remap_storage.segments[i].entries)
                free(ctx->data_remap_storage.segments[i].entries);
        }
    }
    if (ctx->data_remap_storage.segments)
        free(ctx->data_remap_storage.segments);

    if (ctx->meta_remap_storage.header) {
        for (unsigned int i = 0; i < ctx->meta_remap_storage.header->map_segment_count; i++) {
            if (ctx->meta_remap_storage.segments && ctx->meta_remap_storage.segments[i].entries)
                free(ctx->meta_remap_storage.segments[i].entries);
        }
    }
    if (ctx->meta_remap_storage.segments)
        free(ctx->meta_remap_storage.segments);

    if (ctx->data_remap_storage.map_entries)
        free(ctx->data_remap_storage.map_entries);
    if (ctx->meta_remap_storage.map_entries)
        free(ctx->meta_remap_storage.map_entries);

    for (unsigned int i = 0; i < 2; i++) {
        if (ctx->duplex_storage.layers[i].bitmap.bitmap)
            free(ctx->duplex_storage.layers[i].bitmap.bitmap);
        if (ctx->duplex_storage.layers[i].data_a.base_storage.ctx)
            free(ctx->duplex_storage.layers[i].data_a.base_storage.ctx);
        if (ctx->duplex_storage.layers[i].data_b.base_storage.ctx)
            free(ctx->duplex_storage.layers[i].data_b.base_storage.ctx);
    }
    if (ctx->duplex_storage.layers[1].bitmap_storage.base_storage.ctx)
        free(ctx->duplex_storage.layers[1].bitmap_storage.base_storage.ctx);

    if (ctx->journal_storage.map.map_storage)
        free(ctx->journal_storage.map.map_storage);
    if (ctx->journal_storage.map.entries)
        free(ctx->journal_storage.map.entries);

    for (unsigned int i = 0; i < 4; i++) {
        if (ctx->core_data_ivfc_storage.integrity_storages[i].block_validities)
            free(ctx->core_data_ivfc_storage.integrity_storages[i].block_validities);
        save_cached_storage_finalize(&ctx->core_data_ivfc_storage.levels[i + 1]);
    }
    if (ctx->core_data_ivfc_storage.level_validities)
        free(ctx->core_data_ivfc_storage.level_validities);

    if (ctx->header.layout.version >= VERSION_DISF_5) {
        for (unsigned int i = 0; i < 3; i++) {
            if (ctx->fat_ivfc_storage.integrity_storages[i].block_validities)
                free(ctx->fat_ivfc_storage.integrity_storages[i].block_validities);
            save_cached_storage_finalize(&ctx->fat_ivfc_storage.levels[i + 1]);
        }
    }
    if (ctx->fat_ivfc_storage.level_validities)
        free(ctx->fat_ivfc_storage.level_validities);

    if (ctx->fat_storage)
        free(ctx->fat_storage);
}

static ALWAYS_INLINE bool save_flush(save_ctx_t *ctx) {
    if (ctx->header.layout.version < VERSION_DISF_5) {
        if (!save_cached_storage_flush(ctx->core_data_ivfc_storage.data_level)) {
            EPRINTF("Failed to flush cached storage!");
        }
        if (save_remap_storage_write(&ctx->meta_remap_storage, ctx->fat_storage, ctx->header.layout.fat_offset, ctx->header.layout.fat_size) != ctx->header.layout.fat_size) {
            EPRINTF("Failed to write meta remap storage!");
        }
    } else {
        if (!save_cached_storage_flush(ctx->fat_ivfc_storage.data_level)) {
            EPRINTF("Failed to flush cached storage!");
        }
        if (save_remap_storage_write(&ctx->meta_remap_storage, ctx->fat_storage, fs_int64_get(&ctx->header.version_5.fat_ivfc_header.level_hash_info.level_headers[2].logical_offset), ctx->fat_ivfc_storage.length) != ctx->fat_ivfc_storage.length) {
            EPRINTF("Failed to write meta remap storage!");
        }
    }
    return save_hierarchical_duplex_storage_flush(&ctx->duplex_storage, &ctx->data_remap_storage, &ctx->header);
}

bool save_commit(save_ctx_t *ctx) {
    if (!save_flush(ctx)) {
        EPRINTF("Failed to flush save!");
        return false;
    }

    uint32_t hashed_data_offset = sizeof(ctx->header.layout) + sizeof(ctx->header.cmac) + sizeof(ctx->header._0x10);
    uint32_t hashed_data_size = sizeof(ctx->header) - hashed_data_offset;
    uint8_t *header = (uint8_t *)&ctx->header;
    se_calc_sha256_oneshot(ctx->header.layout.hash, header + hashed_data_offset, hashed_data_size);

    se_aes_key_set(10, ctx->save_mac_key, 0x10);
    se_aes_cmac(10, ctx->header.cmac, 0x10, &ctx->header.layout, sizeof(ctx->header.layout));

    if (substorage_write(&ctx->base_storage, &ctx->header, 0, sizeof(ctx->header)) != sizeof(ctx->header)) {
        EPRINTF("Failed to write save header!");
        return false;
    }

    return true;
}
