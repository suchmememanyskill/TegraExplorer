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

#ifndef _SAVE_DATA_FILE_H_
#define _SAVE_DATA_FILE_H_

#include "allocation_table_storage.h"
#include "hierarchical_save_file_table.h"

#include <utils/types.h>

#include <stdint.h>

typedef struct {
    allocation_table_storage_ctx_t base_storage;
    const char *path;
    hierarchical_save_file_table_ctx_t *file_table;
    uint64_t size;
    open_mode_t mode;
} save_data_file_ctx_t;

static ALWAYS_INLINE void save_data_file_get_size(save_data_file_ctx_t *ctx, uint64_t *out_size) {
    *out_size = ctx->size;
}

void save_data_file_init(save_data_file_ctx_t *ctx, allocation_table_storage_ctx_t *base_storage, const char *path, hierarchical_save_file_table_ctx_t *file_table, uint64_t size, open_mode_t mode);
bool save_data_file_read(save_data_file_ctx_t *ctx, uint64_t *out_bytes_read, uint64_t offset, void *buffer, uint64_t count);
bool save_data_file_write(save_data_file_ctx_t *ctx, uint64_t *out_bytes_written, uint64_t offset, const void *buffer, uint64_t count);
bool save_data_file_set_size(save_data_file_ctx_t *ctx, uint64_t size);

#endif