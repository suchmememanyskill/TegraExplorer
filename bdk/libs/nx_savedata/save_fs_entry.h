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

#ifndef _SAVE_FS_ENTRY_H_
#define _SAVE_FS_ENTRY_H_

#include "fs_int64.h"

#include <assert.h>
#include <stdint.h>

typedef struct {
    char name[0x40];
    uint32_t parent;
} save_entry_key_t;

static_assert(sizeof(save_entry_key_t) == 0x44, "Save entry key size is wrong!");

typedef struct {
    uint32_t start_block;
    fs_int64_t length;
    uint32_t _0xC[2];
} save_file_info_t;

static_assert(sizeof(save_file_info_t) == 0x14, "Save file info size is wrong!");

typedef struct {
    uint32_t next_directory;
    uint32_t next_file;
    uint32_t _0x8[3];
} save_find_position_t;

static_assert(sizeof(save_find_position_t) == 0x14, "Save find position size is wrong!");

#endif
