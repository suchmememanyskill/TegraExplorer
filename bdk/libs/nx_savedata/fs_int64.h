/*
 * Copyright (c) 2018-2020 Atmosphère-NX
 * Copyright (c) 2020 shchmue
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

#ifndef _FS_INT64_H_
#define _FS_INT64_H_

#include <utils/types.h>

#include <stdint.h>

/* For 64-bit integers which are 4-byte aligned but not 8-byte aligned. */
typedef struct {
    uint32_t low;
    uint32_t high;
} fs_int64_t;

static ALWAYS_INLINE void fs_int64_set(fs_int64_t *i, int64_t val) {
    i->low  = (uint32_t)((val & (uint64_t)(0x00000000FFFFFFFFul)) >>  0);
    i->high = (uint32_t)((val & (uint64_t)(0xFFFFFFFF00000000ul)) >> 32);
}

static ALWAYS_INLINE const int64_t fs_int64_get(fs_int64_t *i) {
    return ((int64_t)(i->high) << 32) | ((int64_t)i->low);
}

#endif
