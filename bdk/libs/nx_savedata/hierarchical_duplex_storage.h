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

#ifndef _HIER_DUPLEX_STORAGE_H_
#define _HIER_DUPLEX_STORAGE_H_

#include "duplex_storage.h"
#include "header.h"
#include "remap_storage.h"
#include "storage.h"

#include <stdint.h>

typedef struct {
    duplex_storage_ctx_t layers[2];
    duplex_storage_ctx_t *data_layer;
    uint64_t _length;
    substorage storage;
} hierarchical_duplex_storage_ctx_t;

typedef struct {
    uint8_t *data_a;
    uint8_t *data_b;
    duplex_info_t info;
} duplex_fs_layer_info_t;

bool save_hierarchical_duplex_storage_init(hierarchical_duplex_storage_ctx_t *ctx, remap_storage_ctx_t *storage, save_header_t *header);
bool save_hierarchical_duplex_storage_flush(hierarchical_duplex_storage_ctx_t *ctx, remap_storage_ctx_t *storage, save_header_t *header);

#endif
