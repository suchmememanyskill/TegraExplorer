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

#include "allocation_table.h"
#include "allocation_table_iterator.h"

#include <gfx_utils.h>

void save_allocation_table_init(allocation_table_ctx_t *ctx, void *storage, allocation_table_header_t *header) {
    ctx->base_storage = storage;
    ctx->header = header;
    ctx->free_list_entry_index = 0;
}

void save_allocation_table_set_free_list_entry_index(allocation_table_ctx_t *ctx, uint32_t head_block_index) {
    allocation_table_entry_t free_list = {0, head_block_index};
    save_allocation_table_write_entry(ctx, ctx->free_list_entry_index, &free_list);
}

void save_allocation_table_set_free_list_block_index(allocation_table_ctx_t *ctx, uint32_t head_block_index) {
    save_allocation_table_set_free_list_entry_index(ctx, allocation_table_block_to_entry_index(head_block_index));
}

uint32_t save_allocation_table_get_list_tail(allocation_table_ctx_t *ctx, uint32_t entry_index) {
    uint32_t tail_index = entry_index;
    uint32_t table_size = ctx->header->fat_storage_info.count;
    uint32_t nodes_traversed = 0;

    allocation_table_entry_t *entry = save_allocation_table_read_entry(ctx, entry_index);

    while (!allocation_table_is_list_end(entry)) {
        nodes_traversed++;
        tail_index = allocation_table_get_next(entry);
        entry = save_allocation_table_read_entry(ctx, tail_index);
        if (nodes_traversed > table_size) {
            EPRINTF("Cycle detected in allocation table!");
            return 0xFFFFFFFF;
        }
    }

    return tail_index;
}

bool save_allocation_table_split(allocation_table_ctx_t *ctx, uint32_t segment_block_index, uint32_t first_sub_segment_length) {
    uint32_t seg_a_index = allocation_table_block_to_entry_index(segment_block_index);

    allocation_table_entry_t *seg_a = save_allocation_table_read_entry(ctx, seg_a_index);

    if (!allocation_table_is_multi_block_segment(seg_a)) {
        EPRINTF("Cannot split a single-entry segment!");
        return false;
    }

    allocation_table_entry_t *seg_a_range = save_allocation_table_read_entry(ctx, seg_a_index + 1);
    uint32_t original_length = allocation_table_get_next(seg_a_range) - allocation_table_get_prev(seg_a_range) + 1;

    if (first_sub_segment_length >= original_length) {
        EPRINTFARGS("Requested sub-segment length (%x) must\n be less than the full segment length (%x)!", first_sub_segment_length, original_length);
        return false;
    }

    uint32_t seg_b_index = seg_a_index + first_sub_segment_length;
    uint32_t seg_a_length = first_sub_segment_length;
    uint32_t seg_b_length = original_length - seg_a_length;

    allocation_table_entry_t seg_b = {seg_a_index, allocation_table_get_next(seg_a)};
    allocation_table_set_next(seg_a, seg_b_index);

    if (!allocation_table_is_list_end(&seg_b)) {
        allocation_table_entry_t *seg_c = save_allocation_table_read_entry(ctx, allocation_table_get_next(&seg_b));
        allocation_table_set_prev(seg_c, seg_b_index);
    }

    if (seg_b_length > 1) {
        allocation_table_make_multi_block_segment(&seg_b);
        allocation_table_entry_t seg_b_range;
        allocation_table_set_range(&seg_b_range, seg_b_index, seg_b_index + seg_b_length - 1);
        save_allocation_table_write_entry(ctx, seg_b_index + 1, &seg_b_range);
        save_allocation_table_write_entry(ctx, seg_b_index + seg_b_length - 1, &seg_b_range);
    }

    save_allocation_table_write_entry(ctx, seg_b_index, &seg_b);

    if (seg_a_length == 1) {
        allocation_table_make_single_block_segment(seg_a);
    } else {
        allocation_table_set_range(seg_a_range, seg_a_index, seg_a_index + seg_a_length - 1);
        save_allocation_table_write_entry(ctx, seg_a_index + seg_a_length - 1, seg_a_range);
    }

    return true;
}

uint32_t save_allocation_table_trim(allocation_table_ctx_t *ctx, uint32_t list_head_block_index, uint32_t new_list_length) {
    uint32_t blocks_remaining = new_list_length;
    uint32_t next_entry = allocation_table_block_to_entry_index(list_head_block_index);
    uint32_t list_a_index = 0xFFFFFFFF;
    uint32_t list_b_index = 0xFFFFFFFF;

    while (blocks_remaining > 0) {
        if (next_entry == 0)
            return 0xFFFFFFFF;

        uint32_t current_entry_index = next_entry;

        allocation_table_entry_t entry;
        uint32_t segment_length = save_allocation_table_read_entry_with_length(ctx, allocation_table_entry_index_to_block(current_entry_index), &entry);
        if (segment_length == 0) {
            EPRINTF("Invalid entry detected in allocation table!");
            return 0xFFFFFFFF;
        }

        next_entry = allocation_table_block_to_entry_index(entry.next);

        if (segment_length == blocks_remaining) {
            list_a_index = current_entry_index;
            list_b_index = next_entry;
        } else if (segment_length > blocks_remaining) {
            if (!save_allocation_table_split(ctx, allocation_table_entry_index_to_block(current_entry_index), blocks_remaining))
                return 0xFFFFFFFF;
            list_a_index = current_entry_index;
            list_b_index = current_entry_index + blocks_remaining;
            segment_length = blocks_remaining;
        }

        blocks_remaining -= segment_length;
    }

    if (list_a_index == 0xFFFFFFFF || list_b_index == 0xFFFFFFFF)
        return 0xFFFFFFFF;

    allocation_table_entry_t *list_a_node = save_allocation_table_read_entry(ctx, list_a_index);
    allocation_table_entry_t *list_b_node = save_allocation_table_read_entry(ctx, list_b_index);

    allocation_table_set_next(list_a_node, 0);
    allocation_table_make_list_start(list_b_node);

    return allocation_table_entry_index_to_block(list_b_index);
}

bool save_allocation_table_join(allocation_table_ctx_t *ctx, uint32_t front_list_block_index, uint32_t back_list_block_index) {
    uint32_t front_entry_index = allocation_table_block_to_entry_index(front_list_block_index);
    uint32_t back_entry_index = allocation_table_block_to_entry_index(back_list_block_index);

    uint32_t front_tail_index = save_allocation_table_get_list_tail(ctx, front_entry_index);
    if (front_tail_index == 0xFFFFFFFF)
        return false;

    allocation_table_entry_t *front_tail = save_allocation_table_read_entry(ctx, front_tail_index);
    allocation_table_entry_t *back_head = save_allocation_table_read_entry(ctx, back_entry_index);

    allocation_table_set_next(front_tail, back_entry_index);
    allocation_table_set_prev(back_head, front_tail_index);

    return true;
}

uint32_t save_allocation_table_allocate(allocation_table_ctx_t *ctx, uint32_t block_count) {
    uint32_t free_list = save_allocation_table_get_free_list_block_index(ctx);
    uint32_t new_free_list = save_allocation_table_trim(ctx, free_list, block_count);
    if (new_free_list == 0xFFFFFFFF)
        return 0xFFFFFFFF;

    save_allocation_table_set_free_list_block_index(ctx, new_free_list);

    return free_list;
}

bool save_allocation_table_free(allocation_table_ctx_t *ctx, uint32_t list_block_index) {
    uint32_t list_entry_index = allocation_table_block_to_entry_index(list_block_index);
    allocation_table_entry_t *list_entry = save_allocation_table_read_entry(ctx, list_entry_index);

    if (!allocation_table_is_list_start(list_entry)) {
        EPRINTF("The block to free must be the start of a list!");
        return false;
    }

    uint32_t free_list_index = save_allocation_table_get_free_list_entry_index(ctx);

    if (free_list_index == 0) {
        save_allocation_table_set_free_list_entry_index(ctx, list_entry_index);
        return true;
    }

    save_allocation_table_join(ctx, list_block_index, allocation_table_entry_index_to_block(free_list_index));
    save_allocation_table_set_free_list_block_index(ctx, list_block_index);

    return true;
}

uint32_t save_allocation_table_read_entry_with_length(allocation_table_ctx_t *ctx, uint32_t block_index, allocation_table_entry_t *entry) {
    uint32_t length;
    uint32_t entry_index = allocation_table_block_to_entry_index(block_index);

    allocation_table_entry_t *entries = save_allocation_table_read_entry(ctx, entry_index);
    if (allocation_table_is_single_block_segment(&entries[0])) {
        length = 1;
        if (allocation_table_is_range_entry(&entries[0])) {
            EPRINTF("Invalid range entry in allocation table!");
            return 0;
        }
    } else {
        length = entries[1].next - entry_index + 1;
    }

    if (allocation_table_is_list_end(&entries[0])) {
        entry->next = 0xFFFFFFFF;
    } else {
        entry->next = allocation_table_entry_index_to_block(allocation_table_get_next(&entries[0]));
    }

    if (allocation_table_is_list_start(&entries[0])) {
        entry->prev = 0xFFFFFFFF;
    } else {
        entry->prev = allocation_table_entry_index_to_block(allocation_table_get_prev(&entries[0]));
    }

    return length;
}

uint32_t save_allocation_table_get_list_length(allocation_table_ctx_t *ctx, uint32_t block_index) {
    allocation_table_entry_t entry = {0, block_index};
    uint32_t total_length = 0;
    uint32_t table_size = ctx->header->fat_storage_info.count;
    uint32_t nodes_iterated = 0;

    while (entry.next != 0xFFFFFFFF) {
        uint32_t length = save_allocation_table_read_entry_with_length(ctx, entry.next, &entry);
        if (length == 0) {
            EPRINTF("Invalid entry detected in allocation table!");
            return 0;
        }
        total_length += length;
        nodes_iterated++;
        if (nodes_iterated > table_size) {
            EPRINTF("Cycle detected in allocation table!");
            return 0;
        }
    }
    return total_length;
}

uint32_t save_allocation_table_get_free_list_length(allocation_table_ctx_t *ctx) {
    uint32_t free_list_start = save_allocation_table_get_free_list_block_index(ctx);

    if (free_list_start == 0xFFFFFFFF)
        return 0;

    return save_allocation_table_get_list_length(ctx, free_list_start);
}
