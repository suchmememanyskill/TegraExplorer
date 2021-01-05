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

#include "hierarchical_integrity_verification_storage.h"

#include "header.h"

#include <mem/heap.h>
#include <sec/se.h>

#include <string.h>

void save_hierarchical_integrity_verification_storage_control_area_query_size(ivfc_size_set_t *out, const ivfc_storage_control_input_param_t *input_param, int32_t layer_count, uint64_t data_size) {
    int64_t level_size[IVFC_MAX_LEVEL + 1];
    int32_t level = layer_count - 1;

    out->control_size = sizeof(ivfc_save_hdr_t);

    level_size[level] = ALIGN(data_size, input_param->level_block_size[level - 1]);
    level--;

    for ( ; level > 0; --level) {
        level_size[level] = ALIGN(level_size[level + 1] / input_param->level_block_size[level] * 0x20, input_param->level_block_size[level - 1]);
    }

    level_size[0] = 0x20 * level_size[1] / input_param->level_block_size[0];
    out->master_hash_size = level_size[0];

    for (level = 1; level < layer_count - 1; ++level) {
        out->layered_hash_sizes[level - 1] = level_size[level];
    }
}

bool save_hierarchical_integrity_verification_storage_control_area_expand(substorage *storage, const ivfc_save_hdr_t *header) {
    return substorage_write(storage, header, 0, sizeof(ivfc_save_hdr_t)) == sizeof(ivfc_save_hdr_t);
}

void save_hierarchical_integrity_verification_storage_init(hierarchical_integrity_verification_storage_ctx_t *ctx, integrity_verification_info_ctx_t *level_info, uint64_t num_levels, int integrity_check_level) {
    ctx->integrity_check_level = integrity_check_level;
    ctx->level_validities = malloc(sizeof(validity_t *) * (num_levels - 1));
    memcpy(&ctx->levels[0].base_storage, &level_info[0].data, sizeof(substorage));
    for (unsigned int i = 1; i < num_levels; i++) {
        integrity_verification_storage_ctx_t *level_data = &ctx->integrity_storages[i - 1];
        save_ivfc_storage_init(level_data, &level_info[i], &ctx->levels[i - 1].base_storage, integrity_check_level);

        uint64_t level_size = level_data->base_storage.length;
        uint32_t cache_count = MIN((uint32_t)(DIV_ROUND_UP(level_size, level_info[i].block_size)), 4);
        save_cached_storage_init_from_sector_storage(&ctx->levels[i], &level_data->base_storage, cache_count);
        substorage_init(&ctx->levels[i].base_storage, &ivfc_storage_vt, level_data, 0, level_info[i].data.length);

        ctx->level_validities[i - 1] = level_data->block_validities;
    }
    ctx->data_level = &ctx->levels[num_levels - 1];
    ctx->length = ctx->data_level->length;
    substorage_init(&ctx->base_storage, &hierarchical_integrity_verification_storage_vt, ctx, 0, ctx->length);
}

void save_hierarchical_integrity_verification_storage_get_ivfc_info(integrity_verification_info_ctx_t *init_info, ivfc_save_hdr_t *header, uint64_t num_levels, substorage *levels) {
    struct salt_source_t {
        char string[64];
        uint32_t length;
    };

    static const struct salt_source_t salt_sources[IVFC_MAX_LEVEL] = {
        {"HierarchicalIntegrityVerificationStorage::Master", 48},
        {"HierarchicalIntegrityVerificationStorage::L1", 44},
        {"HierarchicalIntegrityVerificationStorage::L2", 44},
        {"HierarchicalIntegrityVerificationStorage::L3", 44},
        {"HierarchicalIntegrityVerificationStorage::L4", 44},
        {"HierarchicalIntegrityVerificationStorage::L5", 44}
    };

    memcpy(&init_info[0].data, &levels[0], sizeof(substorage));
    init_info[0].block_size = 0;
    for (unsigned int i = 1; i < num_levels; i++) {
        memcpy(&init_info[i].data, &levels[i], sizeof(substorage));
        init_info[i].block_size = 1 << header->level_hash_info.level_headers[i - 1].block_size;
        se_calc_hmac_sha256(init_info[i].salt, &header->level_hash_info.seed, sizeof(hash_salt_t), salt_sources[i - 1].string, salt_sources[i - 1].length);
    }
}

static void save_hierarchical_integrity_verification_storage_to_storage_list(substorage *levels, ivfc_save_hdr_t *header, substorage *master_hash, substorage *data) {
    memcpy(&levels[0], master_hash, sizeof(substorage));
    for (unsigned int i = 0; i < 3; i++) {
        ivfc_level_hdr_t *level = &header->level_hash_info.level_headers[i];
        substorage_init(&levels[i + 1], &remap_storage_vt, data, fs_int64_get(&level->logical_offset), fs_int64_get(&level->hash_data_size));
    }
}

void save_hierarchical_integrity_verification_storage_init_with_levels(hierarchical_integrity_verification_storage_ctx_t *ctx, ivfc_save_hdr_t *header, uint64_t num_levels, substorage *levels, int integrity_check_level) {
    integrity_verification_info_ctx_t init_info[IVFC_MAX_LEVEL];
    save_hierarchical_integrity_verification_storage_get_ivfc_info(init_info, header, num_levels, levels);
    save_hierarchical_integrity_verification_storage_init(ctx, init_info, num_levels, integrity_check_level);
}

void save_hierarchical_integrity_verification_storage_init_for_fat(hierarchical_integrity_verification_storage_ctx_t *ctx, ivfc_save_hdr_t *header, substorage *master_hash, substorage *data, int integrity_check_level) {
    const uint32_t ivfc_levels = IVFC_MAX_LEVEL - 2;
    substorage levels[ivfc_levels + 1];
    save_hierarchical_integrity_verification_storage_to_storage_list(levels, header, master_hash, data);
    save_hierarchical_integrity_verification_storage_init_with_levels(ctx, header, ivfc_levels, levels, integrity_check_level);
}

validity_t save_hierarchical_integrity_verification_storage_validate(hierarchical_integrity_verification_storage_ctx_t *ctx) {
    validity_t result = VALIDITY_VALID;
    integrity_verification_storage_ctx_t *storage = &ctx->integrity_storages[3];

    uint32_t block_size = storage->base_storage.sector_size;
    uint32_t block_count = (uint32_t)(DIV_ROUND_UP(ctx->length, block_size));

    uint8_t *buffer = malloc(block_size);

    for (unsigned int i = 0; i < block_count; i++) {
        if (ctx->level_validities[3][i] == VALIDITY_UNCHECKED) {
            uint64_t storage_size = storage->base_storage.length;
            uint32_t to_read = MIN((uint32_t)(storage_size - block_size * i), block_size);
            substorage_read(&ctx->data_level->base_storage, buffer, block_size * i, to_read);
        }
        if (ctx->level_validities[3][i] == VALIDITY_INVALID) {
            result = VALIDITY_INVALID;
            break;
        }
    }
    free(buffer);

    return result;
}

void save_hierarchical_integrity_verification_storage_set_level_validities(hierarchical_integrity_verification_storage_ctx_t *ctx) {
    for (unsigned int i = 0; i < IVFC_MAX_LEVEL - 2; i++) {
        validity_t level_validity = VALIDITY_VALID;
        for (unsigned int j = 0; j < ctx->integrity_storages[i].base_storage.sector_count; j++) {
            if (ctx->level_validities[i][j] == VALIDITY_INVALID) {
                level_validity = VALIDITY_INVALID;
                break;
            }
            if (ctx->level_validities[i][j] == VALIDITY_UNCHECKED && level_validity != VALIDITY_INVALID) {
                level_validity = VALIDITY_UNCHECKED;
            }
        }
        ctx->hash_validity[i] = level_validity;
    }
}

