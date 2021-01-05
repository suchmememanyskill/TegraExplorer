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

#ifndef _HIVFC_H_
#define _HIVFC_H_

#include "cached_storage.h"
#include "fs_int64.h"
#include "integrity_verification_storage.h"
#include "storage.h"

#include <utils/types.h>

#include <assert.h>
#include <stdint.h>

#define IVFC_MAX_LEVEL 6

typedef struct {
    fs_int64_t logical_offset;
    fs_int64_t hash_data_size;
    uint32_t block_size;
    uint8_t reserved[4];
} ivfc_level_hdr_t;

typedef struct {
    ivfc_level_hdr_t *hdr;
    validity_t hash_validity;
} ivfc_level_save_ctx_t;

typedef struct {
    uint8_t val[0x20];
} hash_salt_t;

typedef struct {
    uint32_t num_levels;
    ivfc_level_hdr_t level_headers[IVFC_MAX_LEVEL];
    hash_salt_t seed;
} ivfc_level_hash_info_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t master_hash_size;
    ivfc_level_hash_info_t level_hash_info;
} ivfc_save_hdr_t;

static_assert(sizeof(ivfc_save_hdr_t) == 0xC0, "Ivfc header size invalid!");

typedef struct {
    int64_t control_size;
    int64_t master_hash_size;
    int64_t layered_hash_sizes[IVFC_MAX_LEVEL];
} ivfc_size_set_t;

typedef struct {
    uint64_t level_block_size[IVFC_MAX_LEVEL];
} ivfc_storage_control_input_param_t;

typedef struct {
    cached_storage_ctx_t levels[IVFC_MAX_LEVEL - 1];
    cached_storage_ctx_t *data_level;
    int integrity_check_level;
    validity_t **level_validities;
    uint64_t length;
    integrity_verification_storage_ctx_t integrity_storages[IVFC_MAX_LEVEL - 2];
    validity_t hash_validity[IVFC_MAX_LEVEL - 2];
    substorage base_storage;
} hierarchical_integrity_verification_storage_ctx_t;

void save_hierarchical_integrity_verification_storage_control_area_query_size(ivfc_size_set_t *out, const ivfc_storage_control_input_param_t *input_param, int32_t layer_count, uint64_t data_size);
bool save_hierarchical_integrity_verification_storage_control_area_expand(substorage *header_storage, const ivfc_save_hdr_t *header);
void save_hierarchical_integrity_verification_storage_init(hierarchical_integrity_verification_storage_ctx_t *ctx, integrity_verification_info_ctx_t *level_info, uint64_t num_levels, int integrity_check_level);
void save_hierarchical_integrity_verification_storage_init_with_levels(hierarchical_integrity_verification_storage_ctx_t *ctx, ivfc_save_hdr_t *header, uint64_t num_levels, substorage *levels, int integrity_check_level);
void save_hierarchical_integrity_verification_storage_init_for_fat(hierarchical_integrity_verification_storage_ctx_t *ctx, ivfc_save_hdr_t *header, substorage *master_hash, substorage *data, int integrity_check_level);
validity_t save_hierarchical_integrity_verification_storage_validate(hierarchical_integrity_verification_storage_ctx_t *ctx);
void save_hierarchical_integrity_verification_storage_set_level_validities(hierarchical_integrity_verification_storage_ctx_t *ctx);

#endif
