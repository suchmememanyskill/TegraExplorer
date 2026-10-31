/*
 * Copyright (c) Atmosphère-NX
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

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <utils/types.h>

typedef struct {
    const u8 *src;
    size_t src_size;
    size_t src_offset;

    u8 *dst;
    size_t dst_size;
    size_t dst_offset;
} Lz4Uncompressor;

static inline u8 ReadByte(Lz4Uncompressor *c) {
    return c->src[c->src_offset++];
}

static inline int CanRead(Lz4Uncompressor *c) {
    return c->src_offset < c->src_size;
}

static size_t GetCopySize(Lz4Uncompressor *c, u8 control) {
    size_t size = control;

    if (control >= 0xF) {
        do {
            if (!CanRead(c)) break;
            control = ReadByte(c);
            size += control;
        } while (control == 0xFF);
    }

    return size;
}

static void CopyBytes(Lz4Uncompressor *c, size_t size) {
    memcpy(c->dst + c->dst_offset,
           c->src + c->src_offset,
           size);

    c->dst_offset += size;
    c->src_offset += size;
}

static void _uncompress(Lz4Uncompressor *c) {
    while (1) {
        if (!CanRead(c)) break;
        u8 control = ReadByte(c);

        size_t lit_size = GetCopySize(c, control >> 4);
        CopyBytes(c, lit_size);

        if (c->src_offset >= c->src_size) {
            break;
        }

        if (!CanRead(c)) break;
        u16 offset = ReadByte(c);

        if (!CanRead(c)) break;
        offset |= ((u16)ReadByte(c)) << 8;

        size_t match_size = GetCopySize(c, control & 0xF);
        size_t end_offset = c->dst_offset + match_size + 4;

        for (size_t cur = c->dst_offset; cur < end_offset; ++cur) {
            c->dst[cur] = c->dst[cur - offset];
            c->dst_offset = cur + 1;
        }
    }
}

void Lz4Uncompress(void *dst, size_t dst_size, const void *src, size_t src_size) {
    Lz4Uncompressor ctx;

    ctx.src = (const u8 *)src;
    ctx.src_size = src_size;
    ctx.src_offset = 0;

    ctx.dst = (u8 *)dst;
    ctx.dst_size = dst_size;
    ctx.dst_offset = 0;

    _uncompress(&ctx);
}
