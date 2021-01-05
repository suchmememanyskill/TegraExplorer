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

#ifndef _JOURNAL_STORAGE_H_
#define _JOURNAL_STORAGE_H_

#include "hierarchical_integrity_verification_storage.h"
#include "journal_map.h"
#include "storage.h"

#include <assert.h>
#include <stdint.h>

typedef struct {
    uint32_t magic; /* JNGL */
    uint32_t version;
    uint64_t total_size;
    uint64_t journal_size;
    uint64_t block_size;
    journal_map_header_t map_header;
    uint8_t reserved[0x1D0];
} journal_header_t;

static_assert(sizeof(journal_header_t) == 0x200, "Journal storage header size is wrong!");

typedef struct {
    journal_map_ctx_t map;
    journal_header_t *header;
    uint32_t block_size;
    uint64_t length;
    substorage base_storage;
} journal_storage_ctx_t;

void save_journal_storage_init(journal_storage_ctx_t *ctx, substorage *base_storage, journal_header_t *header, journal_map_params_t *map_info);
uint32_t save_journal_storage_read(journal_storage_ctx_t *ctx, void *buffer, uint64_t offset, uint64_t count);
uint32_t save_journal_storage_write(journal_storage_ctx_t *ctx, const void *buffer, uint64_t offset, uint64_t count);

#endif
