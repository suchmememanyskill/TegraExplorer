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

#ifndef _HEADER_H_
#define _HEADER_H_

#include "allocation_table.h"
#include "duplex_storage.h"
#include "fs_int64.h"
#include "hierarchical_integrity_verification_storage.h"
#include "journal_map.h"
#include "journal_storage.h"
#include "remap_storage.h"

#include <assert.h>
#include <stdint.h>

#define MAGIC_DISF 0x46534944
#define MAGIC_DPFS 0x53465044
#define MAGIC_JNGL 0x4C474E4A
#define MAGIC_SAVE 0x45564153
#define MAGIC_RMAP 0x50414D52
#define MAGIC_IVFC 0x43465649

#define VERSION_DISF_LEEGACY 0x40000
#define VERSION_DISF_5       0x50000
#define VERSION_DPFS         0x10000
#define VERSION_JNGL         0x10000
#define VERSION_SAVE         0x60000
#define VERSION_RMAP         0x10000
#define VERSION_IVFC         0x20000

#define SAVE_BLOCK_SIZE_DEFAULT SZ_16K

#define SAVE_NUM_HEADERS 2

typedef struct {
    uint32_t magic; /* DISF */
    uint32_t version;
    uint8_t hash[0x20];
    uint64_t file_map_entry_offset;
    uint64_t file_map_entry_size;
    uint64_t meta_map_entry_offset;
    uint64_t meta_map_entry_size;
    uint64_t file_map_data_offset;
    uint64_t file_map_data_size;
    uint64_t duplex_l1_offset_a;
    uint64_t duplex_l1_offset_b;
    uint64_t duplex_l1_size;
    uint64_t duplex_data_offset_a;
    uint64_t duplex_data_offset_b;
    uint64_t duplex_data_size;
    uint64_t journal_data_offset;
    uint64_t journal_data_size_a;
    uint64_t journal_data_size_b;
    uint64_t journal_size;
    uint64_t duplex_master_offset_a;
    uint64_t duplex_master_offset_b;
    uint64_t duplex_master_size;
    uint64_t ivfc_master_hash_offset_a;
    uint64_t ivfc_master_hash_offset_b;
    uint64_t ivfc_master_hash_size;
    uint64_t journal_map_table_offset;
    uint64_t journal_map_table_size;
    uint64_t journal_physical_bitmap_offset;
    uint64_t journal_physical_bitmap_size;
    uint64_t journal_virtual_bitmap_offset;
    uint64_t journal_virtual_bitmap_size;
    uint64_t journal_free_bitmap_offset;
    uint64_t journal_free_bitmap_size;
    uint64_t ivfc_l1_offset;
    uint64_t ivfc_l1_size;
    uint64_t ivfc_l2_offset;
    uint64_t ivfc_l2_size;
    uint64_t ivfc_l3_offset;
    uint64_t ivfc_l3_size;
    uint64_t fat_offset;
    uint64_t fat_size;
    uint8_t duplex_index;
    uint64_t fat_ivfc_master_hash_a;
    uint64_t fat_ivfc_master_hash_b;
    uint64_t fat_ivfc_l1_offset;
    uint64_t fat_ivfc_l1_size;
    uint64_t fat_ivfc_l2_offset;
    uint64_t fat_ivfc_l2_size;
    uint8_t _0x190[0x70];
} fs_layout_t;

static_assert(sizeof(fs_layout_t) == 0x200, "Save filesystem layout header size is wrong!");

typedef struct {
    fs_int64_t offset;
    fs_int64_t length;
    uint32_t block_size_power;
} duplex_info_t;

static_assert(sizeof(duplex_info_t) == 0x14, "Duplex info size is wrong!");

typedef enum {
    DUPLEX_LAYER_MASTER = 0,
    DUPLEX_LAYER_1      = 1,
    DUPLEX_LAYER_2      = 2,
} duplex_layer_t;

typedef struct {
    uint32_t magic; /* DPFS */
    uint32_t version;
    duplex_info_t layers[3];
} duplex_header_t;

typedef struct {
    uint64_t level_block_size[2];
} duplex_storage_control_input_param_t;

static_assert(sizeof(duplex_header_t) == 0x44, "Duplex header size is wrong!");

typedef struct {
    uint8_t id[0x10];
} account_user_id_t;

typedef enum {
    SAVE_TYPE_SYSTEM        = 0,
    SAVE_TYPE_ACCOUNT       = 1,
    SAVE_TYPE_BCAT          = 2,
    SAVE_TYPE_DEVICE        = 3,
    SAVE_TYPE_TEMP          = 4,
    SAVE_TYPE_CACHE         = 5,
    SAVE_TYPE_SYSTEM_BCAT   = 6
} save_data_type_t;

typedef enum {
    SAVE_RANK_PRIMARY   = 0,
    SAVE_RANK_SECONDARY = 1,
} save_data_rank_t;

typedef struct {
    uint64_t program_id;
    account_user_id_t user_id;
    uint64_t save_id;
    uint8_t save_data_type;
    uint8_t save_data_rank;
    uint16_t save_data_index;
    uint8_t _0x24[0x1C];
} save_data_attribute_t;

static_assert(sizeof(save_data_attribute_t) == 0x40, "Save data attribute size is wrong!");

typedef enum {
    SAVE_FLAG_KEEP_AFTER_RESETTING_SYSTEM_SAVE_DATA                        = 1,
    SAVE_FLAG_KEEP_AFTER_REFURBISHMENT                                     = 2,
    SAVE_FLAG_KEEP_AFTER_RESETTING_SYSTEM_SAVE_DATA_WITHOUT_USER_SAVE_DATA = 4,
    SAVE_FLAG_NEEDS_SECURE_DELETE                                          = 8,
} save_data_flags_t;

typedef struct {
    save_data_attribute_t save_data_attribute;
    uint64_t save_owner_id;
    uint64_t timestamp;
    uint32_t flags;
    uint8_t _0x54[4];
    uint64_t savedata_size;
    uint64_t journal_size;
    uint64_t commit_id;
    uint8_t reserved[0x190];
} extra_data_t;

static_assert(sizeof(extra_data_t) == 0x200, "Extra data size is wrong!");

typedef struct {
    uint32_t magic; /* SAVE */
    uint32_t version;
    uint64_t block_count;
    uint64_t block_size;
    allocation_table_header_t fat_header;
} save_fs_header_t;

static_assert(sizeof(save_fs_header_t) == 0x48, "Save filesystem header size is wrong!");

typedef struct {
    uint8_t cmac[0x10];
    uint8_t _0x10[0xF0];
    fs_layout_t layout;
    duplex_header_t duplex_header;
    ivfc_save_hdr_t data_ivfc_header;
    uint32_t _0x404;
    journal_header_t journal_header;
    save_fs_header_t save_header;
    remap_header_t main_remap_header, meta_remap_header;
    uint64_t _0x6D0;
    extra_data_t extra_data_a;
    extra_data_t extra_data_b;
    union {
        struct {
            ivfc_save_hdr_t fat_ivfc_header;
            uint64_t _0xB98;
            uint8_t additional_data[0x3460];
        } version_5;
        struct {
            uint8_t additional_data[0x3528];
        } legacy;
    };
} save_header_t;

static_assert(sizeof(save_header_t) == SZ_16K, "Save header size is wrong!");

#endif
