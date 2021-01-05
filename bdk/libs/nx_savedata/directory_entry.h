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

#ifndef _DIRECTORY_ENTRY_H_
#define _DIRECTORY_ENTRY_H_

#include <stdint.h>

typedef enum {
    OPEN_DIR_MODE_DIR           = 1,
    OPEN_DIR_MODE_FILE          = 2,
    OPEN_DIR_MODE_NO_FILE_SIZE  = -2147483648,
    OPEN_DIR_MODE_ALL           = OPEN_DIR_MODE_DIR | OPEN_DIR_MODE_FILE
} open_directory_mode_t;

typedef enum {
    DIR_ENT_TYPE_DIR = 0,
    DIR_ENT_TYPE_FILE
} directory_entry_type_t;

typedef struct {
    char name[0x301];
    uint8_t attributes;
    uint8_t _0x302[2];
    directory_entry_type_t type;
    uint64_t size;
} directory_entry_t;

#endif
