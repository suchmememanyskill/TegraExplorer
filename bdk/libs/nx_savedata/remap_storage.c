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

#include "remap_storage.h"

#include "header.h"

#include <gfx_utils.h>
#include <mem/heap.h>
#include <utils/types.h>

#include <string.h>

remap_segment_ctx_t *save_remap_storage_init_segments(remap_storage_ctx_t *ctx) {
    remap_header_t *header = ctx->header;
    remap_entry_ctx_t *map_entries = ctx->map_entries;

    remap_segment_ctx_t *segments = calloc(1, sizeof(remap_segment_ctx_t) * header->map_segment_count);
    unsigned int entry_idx = 0;

    for (unsigned int i = 0; i < header->map_segment_count; i++) {
        remap_segment_ctx_t *seg = &segments[i];
        seg->entry_count = 0;
        remap_entry_ctx_t **ptr = malloc(sizeof(remap_entry_ctx_t *) * (seg->entry_count + 1));
        if (!ptr) {
            EPRINTF("Failed to allocate entries in remap storage!");
            return NULL;
        }
        seg->entries = ptr;
        seg->entries[seg->entry_count++] = &map_entries[entry_idx];
        seg->offset = map_entries[entry_idx].entry.virtual_offset;
        map_entries[entry_idx++].segment = seg;

        while (entry_idx < header->map_entry_count && map_entries[entry_idx - 1].ends.virtual_offset_end == map_entries[entry_idx].entry.virtual_offset) {
            map_entries[entry_idx].segment = seg;
            map_entries[entry_idx - 1].next = &map_entries[entry_idx];
            remap_entry_ctx_t **ptr = calloc(1, sizeof(remap_entry_ctx_t *) * (seg->entry_count + 1));
            if (!ptr) {
                EPRINTF("Failed to allocate entries in remap storage!");
                return NULL;
            }
            memcpy(ptr, seg->entries, sizeof(remap_entry_ctx_t *) * (seg->entry_count));
            free(seg->entries);
            seg->entries = ptr;
            seg->entries[seg->entry_count++] = &map_entries[entry_idx++];
        }
        seg->length = seg->entries[seg->entry_count - 1]->ends.virtual_offset_end - seg->entries[0]->entry.virtual_offset;
    }
    return segments;
}

static ALWAYS_INLINE remap_entry_ctx_t *save_remap_storage_get_map_entry(remap_storage_ctx_t *ctx, uint64_t offset) {
    uint32_t segment_idx = save_remap_get_segment_from_virtual_offset(ctx->header, offset);
    if (segment_idx < ctx->header->map_segment_count) {
        for (unsigned int i = 0; i < ctx->segments[segment_idx].entry_count; i++) {
            if (ctx->segments[segment_idx].entries[i]->ends.virtual_offset_end > offset) {
                return ctx->segments[segment_idx].entries[i];
            }
        }
    }
    EPRINTFARGS("Remap offset %08x%08x out of range!", (uint32_t)(offset >> 32), (uint32_t)(offset & 0xFFFFFFFF));
    return NULL;
}

uint32_t save_remap_storage_read(remap_storage_ctx_t *ctx, void *buffer, uint64_t offset, uint64_t count) {
    remap_entry_ctx_t *entry = NULL;
    entry = save_remap_storage_get_map_entry(ctx, offset);
    if (!entry) {
        EPRINTF("Unexpected failure in remap get entry!");
        return 0;
    }
    uint64_t in_pos = offset;
    uint32_t out_pos = 0;
    uint32_t remaining = (u32)count;

    while (remaining) {
        uint64_t entry_pos = in_pos - entry->entry.virtual_offset;
        uint32_t bytes_to_read = MIN((uint32_t)(entry->ends.virtual_offset_end - in_pos), remaining);

        if (substorage_read(&ctx->base_storage, (uint8_t *)buffer + out_pos, entry->entry.physical_offset + entry_pos, bytes_to_read) != bytes_to_read)
            return 0;

        out_pos += bytes_to_read;
        in_pos += bytes_to_read;
        remaining -= bytes_to_read;

        if (in_pos >= entry->ends.virtual_offset_end) {
            if (!entry->next && remaining) {
                EPRINTF("Unexpected remap entry chain failure!");
                return 0;
            }
            entry = entry->next;
        }
    }
    return out_pos;
}

uint32_t save_remap_storage_write(remap_storage_ctx_t *ctx, const void *buffer, uint64_t offset, uint64_t count) {
    remap_entry_ctx_t *entry = NULL;
    entry = save_remap_storage_get_map_entry(ctx, offset);
    if (!entry) {
        EPRINTF("Unexpected failure in remap get entry!");
        return 0;
    }
    uint64_t in_pos = offset;
    uint32_t out_pos = 0;
    uint32_t remaining = (u32)count;

    while (remaining) {
        uint64_t entry_pos = in_pos - entry->entry.virtual_offset;
        uint32_t bytes_to_write = MIN((uint32_t)(entry->ends.virtual_offset_end - in_pos), remaining);

        if (substorage_write(&ctx->base_storage, (uint8_t *)buffer + out_pos, entry->entry.physical_offset + entry_pos, bytes_to_write) != bytes_to_write)
            return 0;

        out_pos += bytes_to_write;
        in_pos += bytes_to_write;
        remaining -= bytes_to_write;

        if (in_pos >= entry->ends.virtual_offset_end) {
            if (!entry->next && remaining) {
                EPRINTF("Unexpected remap entry chain failure!");
                return 0;
            }
            entry = entry->next;
        }
    }
    return out_pos;
}
