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

#ifndef HACTOOL_STORAGE_H
#define HACTOOL_STORAGE_H

#include <stdint.h>

#include <utils/types.h>

typedef struct {
    uint32_t (*read)(void *ctx, void *buffer, uint64_t offset, uint64_t count);
    uint32_t (*write)(void *ctx, const void *buffer, uint64_t offset, uint64_t count);
    void (*set_size)(void *ctx, uint64_t size);
    void (*get_size)(void *ctx, uint64_t *out_size);
} storage_vt;

typedef struct {
    const storage_vt *vt;
    void *ctx;
} storage;

static void ALWAYS_INLINE storage_init(storage *this, const storage_vt *vt, void *ctx) {
    this->vt = vt;
    this->ctx = ctx;
}

typedef struct {
    uint64_t offset;
    uint64_t length;
    storage base_storage;
} substorage;

void substorage_init(substorage *this, const storage_vt *vt, void *ctx, uint64_t offset, uint64_t length);
bool substorage_init_from_other(substorage *this, const substorage *other, uint64_t offset, uint64_t length);

static ALWAYS_INLINE uint32_t substorage_read(substorage *ctx, void *buffer, uint64_t offset, uint64_t count) {
    return ctx->base_storage.vt->read(ctx->base_storage.ctx, buffer, ctx->offset + offset, count);
}

static ALWAYS_INLINE uint32_t substorage_write(substorage *ctx, const void *buffer, uint64_t offset, uint64_t count) {
    return ctx->base_storage.vt->write(ctx->base_storage.ctx, buffer, ctx->offset + offset, count);
}

static ALWAYS_INLINE void substorage_get_size(substorage *ctx, uint64_t *out_size) {
    ctx->base_storage.vt->get_size(ctx->base_storage.ctx, out_size);
}

typedef struct {
    substorage base_storage;
    uint32_t sector_size;
    uint32_t sector_count;
    uint64_t length;
} sector_storage;

void sector_storage_init(sector_storage *ctx, substorage *base_storage, uint32_t sector_size);

uint32_t save_hierarchical_integrity_verification_storage_read_wrapper(void *ctx, void *buffer, uint64_t offset, uint64_t count);
uint32_t save_hierarchical_integrity_verification_storage_write_wrapper(void *ctx, const void *buffer, uint64_t offset, uint64_t count);
void save_hierarchical_integrity_verification_storage_get_size_wrapper(void *ctx, uint64_t *out_size);

static const storage_vt hierarchical_integrity_verification_storage_vt = {
    save_hierarchical_integrity_verification_storage_read_wrapper,
    save_hierarchical_integrity_verification_storage_write_wrapper,
    NULL,
    save_hierarchical_integrity_verification_storage_get_size_wrapper
};

uint32_t memory_storage_read_wrapper(void *ctx, void *buffer, uint64_t offset, uint64_t count);
uint32_t memory_storage_write_wrapper(void *ctx, const void *buffer, uint64_t offset, uint64_t count);

static const storage_vt memory_storage_vt = {
    memory_storage_read_wrapper,
    memory_storage_write_wrapper,
    NULL,
    NULL
};

uint32_t save_file_read_wrapper(void *ctx, void *buffer, uint64_t offset, uint64_t count);
uint32_t save_file_write_wrapper(void *ctx, const void *buffer, uint64_t offset, uint64_t count);
void save_file_get_size_wrapper(void *ctx, uint64_t *out_size);

static const storage_vt file_storage_vt = {
    save_file_read_wrapper,
    save_file_write_wrapper,
    NULL,
    save_file_get_size_wrapper
};

uint32_t save_remap_storage_read_wrapper(void *ctx, void *buffer, uint64_t offset, uint64_t count);
uint32_t save_remap_storage_write_wrapper(void *ctx, const void *buffer, uint64_t offset, uint64_t count);
void save_remap_storage_get_size_wrapper(void *ctx, uint64_t *out_size);

static const storage_vt remap_storage_vt = {
    save_remap_storage_read_wrapper,
    save_remap_storage_write_wrapper,
    NULL,
    save_remap_storage_get_size_wrapper
};

uint32_t save_journal_storage_read_wrapper(void *ctx, void *buffer, uint64_t offset, uint64_t count);
uint32_t save_journal_storage_write_wrapper(void *ctx, const void *buffer, uint64_t offset, uint64_t count);
void save_journal_storage_get_size_wrapper(void *ctx, uint64_t *out_size);

static const storage_vt journal_storage_vt = {
    save_journal_storage_read_wrapper,
    save_journal_storage_write_wrapper,
    NULL,
    save_journal_storage_get_size_wrapper
};

uint32_t save_ivfc_storage_read_wrapper(void *ctx, void *buffer, uint64_t offset, uint64_t count);
uint32_t save_ivfc_storage_write_wrapper(void *ctx, const void *buffer, uint64_t offset, uint64_t count);
void save_ivfc_storage_get_size_wrapper(void *ctx, uint64_t *out_size);

static const storage_vt ivfc_storage_vt = {
    save_ivfc_storage_read_wrapper,
    save_ivfc_storage_write_wrapper,
    NULL,
    save_ivfc_storage_get_size_wrapper
};

uint32_t save_hierarchical_duplex_storage_read_wrapper(void *ctx, void *buffer, uint64_t offset, uint64_t count);
uint32_t save_hierarchical_duplex_storage_write_wrapper(void *ctx, const void *buffer, uint64_t offset, uint64_t count);
void save_hierarchical_duplex_storage_get_size_wrapper(void *ctx, uint64_t *out_size);

static const storage_vt hierarchical_duplex_storage_vt = {
    save_hierarchical_duplex_storage_read_wrapper,
    save_hierarchical_duplex_storage_write_wrapper,
    NULL,
    save_hierarchical_duplex_storage_get_size_wrapper
};

static ALWAYS_INLINE bool is_range_valid(uint64_t offset, uint64_t size, uint64_t total_size) {
    return  offset >= 0 &&
            size >= 0 &&
            size <= total_size &&
            offset <= total_size - size;
}

#endif
