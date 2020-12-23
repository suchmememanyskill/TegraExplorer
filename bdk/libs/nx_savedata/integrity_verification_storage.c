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

#include "integrity_verification_storage.h"

#include <gfx_utils.h>
#include <mem/heap.h>
#include <sec/se.h>
#include <utils/types.h>

#include <string.h>

void save_ivfc_storage_init(integrity_verification_storage_ctx_t *ctx, integrity_verification_info_ctx_t *info, substorage *hash_storage, int integrity_check_level) {
    sector_storage_init(&ctx->base_storage, &info->data, info->block_size);
    memcpy(&ctx->hash_storage, hash_storage, sizeof(substorage));
    ctx->integrity_check_level = integrity_check_level;
    memcpy(ctx->salt, info->salt, sizeof(ctx->salt));
    ctx->block_validities = calloc(1, sizeof(validity_t) * ctx->base_storage.sector_count);
}

/* buffer must have size count + 0x20 for salt to by copied in at offset 0. */
static ALWAYS_INLINE void save_ivfc_storage_do_hash(integrity_verification_storage_ctx_t *ctx, uint8_t *out_hash, void *buffer, uint64_t count) {
    memcpy(buffer, ctx->salt, sizeof(ctx->salt));
    se_calc_sha256_oneshot(out_hash, buffer, count + sizeof(ctx->salt));
    out_hash[0x1F] |= 0x80;
}

static ALWAYS_INLINE bool is_empty(const void *buffer, uint64_t count) {
    bool empty = true;
    const uint8_t *buf = (const uint8_t *)buffer;
    for (uint64_t i = 0; i < count; i++) {
        if (buf[i] != 0) {
            empty = false;
            break;
        }
    }
    return empty;
}

bool save_ivfc_storage_read(integrity_verification_storage_ctx_t *ctx, void *buffer, uint64_t offset, uint64_t count) {
    if (count > ctx->base_storage.sector_size) {
        EPRINTF("IVFC read exceeds sector size!");
        return false;
    }

    uint64_t block_index = offset / ctx->base_storage.sector_size;

    if (ctx->block_validities[block_index] == VALIDITY_INVALID && ctx->integrity_check_level) {
        EPRINTF("IVFC hash error!");
        return false;
    }

    uint8_t hash_buffer[0x20] __attribute__((aligned(4))) = {0};
    uint64_t hash_pos = block_index * sizeof(hash_buffer);

    if (substorage_read(&ctx->hash_storage, hash_buffer, hash_pos, sizeof(hash_buffer)) != sizeof(hash_buffer))
        return false;

    if (is_empty(hash_buffer, sizeof(hash_buffer))) {
        memset(buffer, 0, count);
        ctx->block_validities[block_index] = VALIDITY_VALID;
        return true;
    }

    uint8_t *data_buffer = calloc(1, ctx->base_storage.sector_size + 0x20);
    if (substorage_read(&ctx->base_storage.base_storage, data_buffer + 0x20, offset - (offset % ctx->base_storage.sector_size), ctx->base_storage.sector_size) != ctx->base_storage.sector_size) {
        free(data_buffer);
        return false;
    }

    memcpy(buffer, data_buffer + 0x20 + (offset % ctx->base_storage.sector_size), count);

    if (ctx->integrity_check_level && ctx->block_validities[block_index] != VALIDITY_UNCHECKED) {
        free(data_buffer);
        return true;
    }

    uint8_t hash[0x20] __attribute__((aligned(4))) = {0};
    save_ivfc_storage_do_hash(ctx, hash, data_buffer, ctx->base_storage.sector_size);
    free(data_buffer);
    if (memcmp(hash_buffer, hash, sizeof(hash_buffer)) == 0) {
        ctx->block_validities[block_index] = VALIDITY_VALID;
    } else {
        ctx->block_validities[block_index] = VALIDITY_INVALID;
        if (ctx->integrity_check_level) {
            EPRINTF("IVFC hash error!");
            return false;
        }
    }

    return true;
}

bool save_ivfc_storage_write(integrity_verification_storage_ctx_t *ctx, const void *buffer, uint64_t offset, uint64_t count) {
    uint64_t block_index = offset / ctx->base_storage.sector_size;
    uint64_t hash_pos = block_index * 0x20;

    uint8_t hash[0x20] __attribute__((aligned(4))) = {0};
    uint8_t *data_buffer = calloc(1, ctx->base_storage.sector_size + 0x20);
    if (count < ctx->base_storage.sector_size) {
        if (substorage_read(&ctx->base_storage.base_storage, data_buffer + 0x20, offset - (offset % ctx->base_storage.sector_size), ctx->base_storage.sector_size) != ctx->base_storage.sector_size) {
            free(data_buffer);
            return false;
        }
    }
    memcpy(data_buffer + 0x20 + (offset % ctx->base_storage.sector_size), buffer, count);

    if (!is_empty(buffer, count)) {
        save_ivfc_storage_do_hash(ctx, hash, data_buffer, ctx->base_storage.sector_size);
    }

    if (substorage_write(&ctx->base_storage.base_storage, data_buffer + 0x20, offset - (offset % ctx->base_storage.sector_size), ctx->base_storage.sector_size) != ctx->base_storage.sector_size) {
        free(data_buffer);
        return false;
    }
    free(data_buffer);
    if (substorage_write(&ctx->hash_storage, hash, hash_pos, sizeof(hash)) != sizeof(hash))
        return false;

    ctx->block_validities[block_index] = VALIDITY_UNCHECKED;

    return true;
}
