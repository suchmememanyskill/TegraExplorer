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

#ifndef _PATH_PARSER_H_
#define _PATH_PARSER_H_

#include <utils/types.h>

#include <stdint.h>

typedef struct {
    const char *_path;
    uint64_t path_len;
    uint32_t _offset;
    uint32_t _length;
    bool _finished;
} path_parser_ctx_t;

static ALWAYS_INLINE bool save_path_parser_is_finished(path_parser_ctx_t *ctx) {
    return ctx->_finished;
}

bool save_path_parser_init(path_parser_ctx_t *ctx, const char *path);
bool save_path_parser_move_next(path_parser_ctx_t *ctx);
const char *save_path_parser_get_current(path_parser_ctx_t *ctx, uint32_t *out_len);
bool save_path_parser_try_get_next(path_parser_ctx_t *ctx, char *name);

#endif
