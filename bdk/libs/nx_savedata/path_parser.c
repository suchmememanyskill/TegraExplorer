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

#include "path_parser.h"

#include <gfx_utils.h>

#include <string.h>

bool save_path_parser_init(path_parser_ctx_t *ctx, const char *path) {
    ctx->path_len = strlen(path);

    if (ctx->path_len < 1 || path[0] != '/') {
        EPRINTF("Path must begin with a '/'!");
        return false;
    }

    ctx->_path = path;
    ctx->_offset = 0;
    ctx->_length = 0;
    ctx->_finished = ctx->path_len == 1 || path[1] == '\0';
    return true;
}

bool save_path_parser_move_next(path_parser_ctx_t *ctx) {
    if (ctx->_finished)
        return false;

    ctx->_offset = ctx->_offset + ctx->_length + 1;
    uint32_t end = ctx->_offset;

    while (end < ctx->path_len && ctx->_path[end] != '\0' && ctx->_path[end] != '/')
        end++;

    ctx->_finished = end + 1 >= ctx->path_len || ctx->_path[end + 1] == '\0';
    ctx->_length = end - ctx->_offset;

    return true;
}

const char *save_path_parser_get_current(path_parser_ctx_t *ctx, uint32_t *out_len) {
    if (out_len)
        *out_len = ctx->_length;
    return &ctx->_path[ctx->_offset];
}

bool save_path_parser_try_get_next(path_parser_ctx_t *ctx, char *name) {
    bool success = save_path_parser_move_next(ctx);
    uint32_t current_len = 0;
    const char *current = save_path_parser_get_current(ctx, &current_len);
    memcpy(name, current, current_len);
    return success;
}
