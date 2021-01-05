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

#ifndef _IVFC_H_
#define _IVFC_H_

#include "storage.h"

#include <stdint.h>

typedef struct {
    substorage hash_storage;
    int integrity_check_level;
    validity_t *block_validities;
    uint8_t salt[0x20];
    sector_storage base_storage;
} integrity_verification_storage_ctx_t;

typedef struct {
    substorage data;
    uint32_t block_size;
    uint8_t salt[0x20];
} integrity_verification_info_ctx_t;

void save_ivfc_storage_init(integrity_verification_storage_ctx_t *ctx, integrity_verification_info_ctx_t *info, substorage *hash_storage, int integrity_check_level);
bool save_ivfc_storage_read(integrity_verification_storage_ctx_t *ctx, void *buffer, uint64_t offset, uint64_t count);
bool save_ivfc_storage_write(integrity_verification_storage_ctx_t *ctx, const void *buffer, uint64_t offset, uint64_t count);

#endif
