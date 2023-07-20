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

#ifndef _REMAP_STORAGE_H_
#define _REMAP_STORAGE_H_

#include "duplex_storage.h"
#include "storage.h"

#include <stdint.h>

#define RMAP_ALIGN_SMALL 0x200
#define RMAP_ALIGN_LARGE SZ_16K

typedef struct {
    uint32_t magic; /* RMAP */
    uint32_t version;
    uint32_t map_entry_count;
    uint32_t map_segment_count;
    uint32_t segment_bits;
    uint8_t _0x14[0x2C];
} remap_header_t;

typedef struct remap_segment_ctx_t remap_segment_ctx_t;
typedef struct remap_entry_ctx_t remap_entry_ctx_t;

typedef struct {
    uint64_t virtual_offset_end;
    uint64_t physical_offset_end;
} remap_end_offsets_t;

typedef struct {
    uint64_t virtual_offset;
    uint64_t physical_offset;
    uint64_t size;
    uint32_t alignment;
    uint32_t _0x1C;
} remap_entry_t;

struct remap_entry_ctx_t {
    remap_entry_t entry;
    remap_end_offsets_t ends;
    remap_segment_ctx_t *segment;
    remap_entry_ctx_t *next;
};

struct remap_segment_ctx_t{
    uint64_t offset;
    uint64_t length;
    remap_entry_ctx_t **entries;
    uint64_t entry_count;
};

typedef struct {
    remap_header_t *header;
    remap_entry_ctx_t *map_entries;
    remap_segment_ctx_t *segments;
    substorage base_storage;
} remap_storage_ctx_t;

static ALWAYS_INLINE uint32_t save_remap_get_segment_from_virtual_offset(remap_header_t *header, uint64_t offset) {
    return (uint32_t)(offset >> (64 - header->segment_bits));
}

static ALWAYS_INLINE uint64_t save_remap_get_virtual_offset(remap_header_t *header, uint64_t segment) {
    return segment << (64 - header->segment_bits);
}

remap_segment_ctx_t *save_remap_storage_init_segments(remap_storage_ctx_t *ctx);
uint32_t save_remap_storage_read(remap_storage_ctx_t *ctx, void *buffer, uint64_t offset, uint64_t count);
uint32_t save_remap_storage_write(remap_storage_ctx_t *ctx, const void *buffer, uint64_t offset, uint64_t count);

#endif
