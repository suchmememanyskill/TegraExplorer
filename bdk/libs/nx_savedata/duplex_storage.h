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

#ifndef _DUPLEX_STORAGE_H_
#define _DUPLEX_STORAGE_H_

#include "storage.h"

#include <stdint.h>

#define DUPLEX_BITMAP_SIZE_BITS  0x200
#define DUPLEX_BITMAP_SIZE_BYTES 0x40

typedef struct {
    uint8_t *data;
    uint8_t *bitmap;
} duplex_bitmap_t;

typedef struct {
    uint32_t block_size;
    substorage bitmap_storage;
    substorage data_a;
    substorage data_b;
    duplex_bitmap_t bitmap;
    uint64_t _length;
    substorage base_storage;
} duplex_storage_ctx_t;

static ALWAYS_INLINE void save_bitmap_set_bit(void *buffer, uint64_t bit_offset) {
    *((uint8_t *)buffer + (bit_offset >> 3)) |= 1 << (bit_offset & 7);
}

static ALWAYS_INLINE void save_bitmap_clear_bit(void *buffer, uint64_t bit_offset) {
    *((uint8_t *)buffer + (bit_offset >> 3)) &= ~(uint8_t)(1 << (bit_offset & 7));
}

static ALWAYS_INLINE uint8_t save_bitmap_check_bit(const void *buffer, uint64_t bit_offset) {
    return *((uint8_t *)buffer + (bit_offset >> 3)) & (1 << (bit_offset & 7));
}

void save_duplex_storage_init(duplex_storage_ctx_t *ctx, uint8_t *data_a, uint8_t *data_b, uint32_t block_size_power, void *bitmap, uint64_t bitmap_size);
uint32_t save_duplex_storage_read(duplex_storage_ctx_t *ctx, void *buffer, uint64_t offset, uint64_t count);
uint32_t save_duplex_storage_write(duplex_storage_ctx_t *ctx, const void *buffer, uint64_t offset, uint64_t count);

#endif
