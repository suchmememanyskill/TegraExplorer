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

#include "cached_storage.h"

#include <gfx_utils.h>
#include <mem/heap.h>

#include <string.h>

static ALWAYS_INLINE cache_block_t *cache_block_init(cached_storage_ctx_t *ctx) {
    cache_block_t *block = calloc(1, sizeof(cache_block_t));
    block->buffer = malloc(ctx->block_size);
    block->index = -1;
    return block;
}

void save_cached_storage_init(cached_storage_ctx_t *ctx, substorage *base_storage, uint32_t block_size, uint32_t cache_size) {
    memcpy(&ctx->base_storage, base_storage, sizeof(substorage));
    ctx->block_size = block_size;
    ctx->length = base_storage->length;
    ctx->cache_size = cache_size;

    list_init(&ctx->blocks);
    for (uint32_t i = 0; i < cache_size; i++) {
        cache_block_t *block = cache_block_init(ctx);
        list_append(&ctx->blocks, &block->link);
    }
}

void save_cached_storage_init_from_sector_storage(cached_storage_ctx_t *ctx, sector_storage *base_storage, uint32_t cache_size) {
    save_cached_storage_init(ctx, &base_storage->base_storage, base_storage->sector_size, cache_size);
}

static void cache_block_finalize(cache_block_t **block) {
    free((*block)->buffer);
    free(*block);
}

void save_cached_storage_finalize(cached_storage_ctx_t *ctx) {
    if (!ctx->blocks.next)
        return;
    LIST_FOREACH_SAFE(curr_block, &ctx->blocks) {
        cache_block_t *block = CONTAINER_OF(curr_block, cache_block_t, link) ;
        cache_block_finalize(&block);
    }
}

static bool try_get_block_by_value(cached_storage_ctx_t *ctx, uint64_t index, cache_block_t **out_block) {
    if (!ctx->blocks.next)
        return false;
    LIST_FOREACH_ENTRY(cache_block_t, block, &ctx->blocks, link) {
        if (block->index == index) {
            *out_block = block;
            return true;
        }
    }
    return false;
}

static bool flush_block(cached_storage_ctx_t *ctx, cache_block_t *block) {
    if (!block->dirty)
        return true;

    uint64_t offset = block->index * ctx->block_size;
    if (substorage_write(&ctx->base_storage, block->buffer, offset, block->length) != block->length) {
        EPRINTF("Cached storage: Failed to write block!");
        return false;
    }
    block->dirty = false;

    return true;
}

static bool read_block(cached_storage_ctx_t *ctx, cache_block_t *block, uint64_t index) {
    uint64_t offset = index * ctx->block_size;
    uint32_t length = ctx->block_size;

    if (ctx->length != -1)
        length = (uint32_t)MIN(ctx->length - offset, length);

    if (substorage_read(&ctx->base_storage, block->buffer, offset, length) != length) {
        EPRINTF("Cached storage: Failed to read block!");
        return false;
    }
    block->length = length;
    block->index = index;
    block->dirty = false;

    return true;
}

static cache_block_t *get_block(cached_storage_ctx_t *ctx, uint64_t block_index) {
    cache_block_t *block = NULL;
    if (try_get_block_by_value(ctx, block_index, &block)) {
        // Promote most recently used block to front of list if not already.
        if (ctx->blocks.next != &block->link) {
            list_remove(&block->link);
            list_prepend(&ctx->blocks, &block->link);
        }
        return block;
    }

    // Get a pointer either to the least recently used block or to a newly allocated block if storage is empty.
    bool block_is_new = false;
    if (ctx->blocks.prev != &ctx->blocks) {
        block = CONTAINER_OF(ctx->blocks.prev, cache_block_t, link);
        if (!flush_block(ctx, block))
            return NULL;

        // Remove least recently used block from list.
        list_remove(&block->link);
    } else {
        block = cache_block_init(ctx);
        block_is_new = true;
    }

    if (!read_block(ctx, block, block_index)) {
        if (block_is_new)
            cache_block_finalize(&block);
        return NULL;
    }

    list_prepend(&ctx->blocks, &block->link);

    return block;
}

uint32_t save_cached_storage_read(cached_storage_ctx_t *ctx, void *buffer, uint64_t offset, uint64_t count) {
    uint64_t remaining = count;
    uint64_t in_offset = offset;
    uint32_t out_offset = 0;

    if (!is_range_valid(offset, count, ctx->length)) {
        EPRINTF("Cached storage read out of range!");
        return 0;
    }

    while (remaining) {
        uint64_t block_index = in_offset / ctx->block_size;
        uint32_t block_pos = (uint32_t)(in_offset % ctx->block_size);
        cache_block_t *block = get_block(ctx,block_index);
        if (!block) {
            EPRINTFARGS("Cached storage read: Unable to get block\n at index %x", (uint32_t)block_index);
            return 0;
        }

        uint32_t bytes_to_read = (uint32_t)MIN(remaining, ctx->block_size - block_pos);

        memcpy((uint8_t *)buffer + out_offset, block->buffer + block_pos, bytes_to_read);

        out_offset += bytes_to_read;
        in_offset += bytes_to_read;
        remaining -= bytes_to_read;
    }

    return out_offset;
}

uint32_t save_cached_storage_write(cached_storage_ctx_t *ctx, const void *buffer, uint64_t offset, uint64_t count) {
    uint64_t remaining = count;
    uint64_t in_offset = offset;
    uint32_t out_offset = 0;

    if (!is_range_valid(offset, count, ctx->length)) {
        EPRINTF("Cached storage write out of range!");
        return 0;
    }

    while (remaining) {
        uint64_t block_index = in_offset / ctx->block_size;
        uint32_t block_pos = (uint32_t)(in_offset % ctx->block_size);
        cache_block_t *block = get_block(ctx,block_index);
        if (!block) {
            EPRINTFARGS("Cached storage write: Unable to get block\n at index %x", (uint32_t)block_index);
            return 0;
        }

        uint32_t bytes_to_write = (uint32_t)MIN(remaining, ctx->block_size - block_pos);

        memcpy(block->buffer + block_pos, (uint8_t *)buffer + out_offset, bytes_to_write);

        block->dirty = true;

        out_offset += bytes_to_write;
        in_offset += bytes_to_write;
        remaining -= bytes_to_write;
    }

    return out_offset;
}

bool save_cached_storage_flush(cached_storage_ctx_t *ctx) {
    LIST_FOREACH_ENTRY(cache_block_t, block, &ctx->blocks, link) {
        if (!flush_block(ctx, block))
            return false;
    }

    return true;
}
