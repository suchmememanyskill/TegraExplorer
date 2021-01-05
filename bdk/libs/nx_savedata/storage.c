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

#include "storage.h"

#include "cached_storage.h"
#include "hierarchical_duplex_storage.h"
#include "hierarchical_integrity_verification_storage.h"
#include "journal_storage.h"
#include "remap_storage.h"

#include <gfx_utils.h>
#include <libs/fatfs/ff.h>

#include <string.h>

void substorage_init(substorage *this, const storage_vt *vt, void *ctx, uint64_t offset, uint64_t length) {
    storage_init(&this->base_storage, vt, ctx);
    this->offset = offset;
    this->length = length;
}

bool substorage_init_from_other(substorage *this, const substorage *other, uint64_t offset, uint64_t length) {
    if (offset + length > other->length) {
        EPRINTF("Invalid size for substorage init!");
        EPRINTFARGS("ofs %x len %x size %x", (uint32_t)offset, (uint32_t)length, (uint32_t)other->length);
        return false;
    }
    substorage_init(this, other->base_storage.vt, other->base_storage.ctx, other->offset + offset, length);
    return true;
}

void sector_storage_init(sector_storage *ctx, substorage *base_storage, uint32_t sector_size) {
    memcpy(&ctx->base_storage, base_storage, sizeof(substorage));
    ctx->sector_size = sector_size;
    ctx->length = base_storage->length;
    ctx->sector_count = (uint32_t)(DIV_ROUND_UP(ctx->length, ctx->sector_size));
}

uint32_t sector_storage_read(sector_storage *ctx, void *buffer, uint64_t offset, uint64_t count) {
    uint64_t remaining = count;
    uint64_t in_offset = offset;
    uint32_t out_offset = 0;
    uint32_t sector_size = ctx->sector_size;

    while (remaining) {
        uint32_t sector_pos = (uint32_t)(in_offset % sector_size);
        uint32_t bytes_to_read = MIN((uint32_t)remaining, (uint32_t)(sector_size - sector_pos));

        substorage_read(&ctx->base_storage, (uint8_t *)buffer + out_offset, in_offset, bytes_to_read);

        out_offset += bytes_to_read;
        in_offset += bytes_to_read;
        remaining -= bytes_to_read;
    }

    return out_offset;
}

uint32_t sector_storage_write(sector_storage *ctx, const void *buffer, uint64_t offset, uint64_t count) {
    uint64_t remaining = count;
    uint64_t in_offset = offset;
    uint32_t out_offset = 0;
    uint32_t sector_size = ctx->sector_size;

    while (remaining) {
        uint32_t sector_pos = (uint32_t)(in_offset % sector_size);
        uint32_t bytes_to_write = MIN((uint32_t)remaining, (uint32_t)(sector_size - sector_pos));

        substorage_write(&ctx->base_storage, (uint8_t *)buffer + out_offset, in_offset, bytes_to_write);

        out_offset += bytes_to_write;
        in_offset += bytes_to_write;
        remaining -= bytes_to_write;
    }

    return out_offset;
}

uint32_t save_hierarchical_integrity_verification_storage_read_wrapper(void *ctx, void *buffer, uint64_t offset, uint64_t count) {
    hierarchical_integrity_verification_storage_ctx_t *storage = (hierarchical_integrity_verification_storage_ctx_t *)ctx;
    return save_cached_storage_read(storage->data_level, buffer, offset, count);
}

uint32_t save_hierarchical_integrity_verification_storage_write_wrapper(void *ctx, const void *buffer, uint64_t offset, uint64_t count) {
    hierarchical_integrity_verification_storage_ctx_t *storage = (hierarchical_integrity_verification_storage_ctx_t *)ctx;
    return save_cached_storage_write(storage->data_level, buffer, offset, count);
}

void save_hierarchical_integrity_verification_storage_get_size_wrapper(void *ctx, uint64_t *out_size) {
    hierarchical_integrity_verification_storage_ctx_t *storage = (hierarchical_integrity_verification_storage_ctx_t *)ctx;
    *out_size = storage->length;
}

uint32_t memory_storage_read(uint8_t *storage, void *buffer, uint64_t offset, uint64_t count) {
    memcpy(buffer, storage + offset, count);
    return count;
}

uint32_t memory_storage_write(uint8_t *storage, const void *buffer, uint64_t offset, uint64_t count) {
    memcpy(storage + offset, buffer, count);
    return count;
}

uint32_t memory_storage_read_wrapper(void *ctx, void *buffer, uint64_t offset, uint64_t count) {
    return memory_storage_read((uint8_t *)ctx, buffer, offset, count);
}

uint32_t memory_storage_write_wrapper(void *ctx, const void *buffer, uint64_t offset, uint64_t count) {
    return memory_storage_write((uint8_t *)ctx, buffer, offset, count);
}

uint32_t save_file_read(FIL *fp, void *buffer, uint64_t offset, uint64_t count) {
    UINT bytes_read = 0;

    if (f_lseek(fp, offset) || f_read(fp, buffer, count, &bytes_read) || bytes_read != count) {
        EPRINTFARGS("Failed to read file at offset %x!\nRead %x bytes. Req %x bytes.", (uint32_t)offset, bytes_read, (uint32_t)count);
        return 0;
    }
    return bytes_read;
}

uint32_t save_file_write(FIL *fp, const void *buffer, uint64_t offset, uint64_t count) {
    UINT bytes_written = 0;
    if (f_lseek(fp, offset) || f_write(fp, buffer, count, &bytes_written) || bytes_written != count) {
        EPRINTFARGS("Failed to write file at offset %x!", (uint32_t)offset);
        return 0;
    }
    return bytes_written;
}

void save_file_get_size(FIL *fp, uint64_t *out_size) {
    *out_size = f_size(fp);
}

uint32_t save_file_read_wrapper(void *ctx, void *buffer, uint64_t offset, uint64_t count) {
    return save_file_read((FIL *)ctx, buffer, offset, count);
}

uint32_t save_file_write_wrapper(void *ctx, const void *buffer, uint64_t offset, uint64_t count) {
    return save_file_write((FIL *)ctx, buffer, offset, count);
}

void save_file_get_size_wrapper(void *ctx, uint64_t *out_size) {
    save_file_get_size((FIL *)ctx, out_size);
}

uint32_t save_remap_storage_read_wrapper(void *ctx, void *buffer, uint64_t offset, uint64_t count) {
    return save_remap_storage_read((remap_storage_ctx_t *)ctx, buffer, offset, count);
}

uint32_t save_remap_storage_write_wrapper(void *ctx, const void *buffer, uint64_t offset, uint64_t count) {
    return save_remap_storage_write((remap_storage_ctx_t *)ctx, buffer, offset, count);
}

void save_remap_storage_get_size_wrapper(__attribute__((unused)) void *ctx, uint64_t *out_size) {
    *out_size = -1;
}

uint32_t save_journal_storage_read_wrapper(void *ctx, void *buffer, uint64_t offset, uint64_t count) {
    return save_journal_storage_read((journal_storage_ctx_t *)ctx, buffer, offset, count);
}

uint32_t save_journal_storage_write_wrapper(void *ctx, const void *buffer, uint64_t offset, uint64_t count) {
    return save_journal_storage_write((journal_storage_ctx_t *)ctx, buffer, offset, count);
}

void save_journal_storage_get_size_wrapper(void *ctx, uint64_t *out_size) {
    journal_storage_ctx_t *journal = (journal_storage_ctx_t *)ctx;
    *out_size = journal->length;
}

uint32_t save_ivfc_storage_read_wrapper(void *ctx, void *buffer, uint64_t offset, uint64_t count) {
    return save_ivfc_storage_read((integrity_verification_storage_ctx_t *)ctx, buffer, offset, count) ? count : 0;
}

uint32_t save_ivfc_storage_write_wrapper(void *ctx, const void *buffer, uint64_t offset, uint64_t count) {
    return save_ivfc_storage_write((integrity_verification_storage_ctx_t *)ctx, buffer, offset, count) ? count : 0;
}

void save_ivfc_storage_get_size_wrapper(void *ctx, uint64_t *out_size) {
    integrity_verification_storage_ctx_t *ivfc = (integrity_verification_storage_ctx_t *)ctx;
    *out_size = ivfc->base_storage.length;
}

uint32_t save_hierarchical_duplex_storage_read(hierarchical_duplex_storage_ctx_t *ctx, void *buffer, uint64_t offset, uint64_t count) {
    return save_duplex_storage_read(ctx->data_layer, buffer, offset, count);
}

uint32_t save_hierarchical_duplex_storage_write(hierarchical_duplex_storage_ctx_t *ctx, const void *buffer, uint64_t offset, uint64_t count) {
    return save_duplex_storage_write(ctx->data_layer, buffer, offset, count);
}

uint32_t save_hierarchical_duplex_storage_read_wrapper(void *ctx, void *buffer, uint64_t offset, uint64_t count) {
    return save_hierarchical_duplex_storage_read((hierarchical_duplex_storage_ctx_t *)ctx, buffer, offset, count);
}

uint32_t save_hierarchical_duplex_storage_write_wrapper(void *ctx, const void *buffer, uint64_t offset, uint64_t count) {
    return save_hierarchical_duplex_storage_write((hierarchical_duplex_storage_ctx_t *)ctx, buffer, offset, count);
}

void save_hierarchical_duplex_storage_get_size_wrapper(void *ctx, uint64_t *out_size) {
    hierarchical_duplex_storage_ctx_t *duplex = (hierarchical_duplex_storage_ctx_t *)ctx;
    *out_size = duplex->_length;
}
