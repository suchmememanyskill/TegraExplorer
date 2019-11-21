#include <string.h>
#include "save.h"

#include "../gfx/gfx.h"
#include "../mem/heap.h"
#include "../sec/se.h"
#include "../utils/types.h"
#include "../utils/util.h"

#define REMAP_ENTRY_LENGTH 0x20

static inline void save_bitmap_set_bit(void *buffer, size_t bit_offset) {
    *((uint8_t *)buffer + (bit_offset >> 3)) |= 1 << (bit_offset & 7);
}

static inline void save_bitmap_clear_bit(void *buffer, size_t bit_offset) {
    *((uint8_t *)buffer + (bit_offset >> 3)) &= ~(uint8_t)(1 << (bit_offset & 7));
}

static inline uint8_t save_bitmap_check_bit(const void *buffer, size_t bit_offset) {
    return *((uint8_t *)buffer + (bit_offset >> 3)) & (1 << (bit_offset & 7));
}

void save_duplex_storage_init(duplex_storage_ctx_t *ctx, duplex_fs_layer_info_t *layer, void *bitmap, uint64_t bitmap_size) {
    ctx->data_a = layer->data_a;
    ctx->data_b = layer->data_b;
    ctx->bitmap_storage = (uint8_t *)bitmap;
    ctx->block_size = 1 << layer->info.block_size_power;

    ctx->bitmap.data = ctx->bitmap_storage;
    ctx->bitmap.bitmap = malloc(bitmap_size >> 3);

    uint32_t bits_remaining = bitmap_size;
    uint32_t bitmap_pos = 0;
    uint32_t *buffer_pos = (uint32_t *)bitmap;
    while (bits_remaining) {
        uint32_t bits_to_read = bits_remaining < 32 ? bits_remaining : 32;
        uint32_t val = *buffer_pos;
        for (uint32_t i = 0; i < bits_to_read; i++) {
            if (val & 0x80000000)
                save_bitmap_set_bit(ctx->bitmap.bitmap, bitmap_pos);
            else
                save_bitmap_clear_bit(ctx->bitmap.bitmap, bitmap_pos);
            bitmap_pos++;
            bits_remaining--;
            val <<= 1;
        }
        buffer_pos++;
    }
}

uint32_t save_duplex_storage_read(duplex_storage_ctx_t *ctx, void *buffer, uint64_t offset, size_t count) {
    uint64_t in_pos = offset;
    uint32_t out_pos = 0;
    uint32_t remaining = count;

    while (remaining) {
        uint32_t block_num = (uint32_t)(in_pos / ctx->block_size);
        uint32_t block_pos = (uint32_t)(in_pos % ctx->block_size);
        uint32_t bytes_to_read = ctx->block_size - block_pos < remaining ? ctx->block_size - block_pos : remaining;

        uint8_t *data = save_bitmap_check_bit(ctx->bitmap.bitmap, block_num) ? ctx->data_b : ctx->data_a;
        memcpy((uint8_t *)buffer + out_pos, data + in_pos, bytes_to_read);

        out_pos += bytes_to_read;
        in_pos += bytes_to_read;
        remaining -= bytes_to_read;
    }
    return out_pos;
}

remap_segment_ctx_t *save_remap_init_segments(remap_header_t *header, remap_entry_ctx_t *map_entries, uint32_t num_map_entries) {
    remap_segment_ctx_t *segments =  malloc(sizeof(remap_segment_ctx_t) * header->map_segment_count);
    unsigned int entry_idx = 0;

    for (unsigned int i = 0; i < header->map_segment_count; i++) {
        remap_segment_ctx_t *seg = &segments[i];
        seg->entries = malloc(sizeof(remap_entry_ctx_t));
        memcpy(seg->entries, &map_entries[entry_idx], sizeof(remap_entry_ctx_t));
        seg->offset = map_entries[entry_idx].virtual_offset;
        map_entries[entry_idx].segment = seg;
        seg->entry_count = 1;
        entry_idx++;

        while (entry_idx < num_map_entries && map_entries[entry_idx - 1].virtual_offset_end == map_entries[entry_idx].virtual_offset) {
            map_entries[entry_idx].segment = seg;
            map_entries[entry_idx - 1].next = &map_entries[entry_idx];
            seg->entries = malloc(sizeof(remap_entry_ctx_t));
            memcpy(seg->entries, &map_entries[entry_idx], sizeof(remap_entry_ctx_t));
            seg->entry_count++;
            entry_idx++;
        }
        seg->length = seg->entries[seg->entry_count - 1].virtual_offset_end - seg->entries[0].virtual_offset;
    }
    return segments;
}

remap_entry_ctx_t *save_remap_get_map_entry(remap_storage_ctx_t *ctx, uint64_t offset) {
    uint32_t segment_idx = (uint32_t)(offset >> (64 - ctx->header->segment_bits));
    if (segment_idx < ctx->header->map_segment_count) {
        for (unsigned int i = 0; i < ctx->segments[segment_idx].entry_count; i++)
            if (ctx->segments[segment_idx].entries[i].virtual_offset_end > offset)
                return &ctx->segments[segment_idx].entries[i];
    }
    return NULL;
}

uint32_t save_remap_read(remap_storage_ctx_t *ctx, void *buffer, uint64_t offset, size_t count) {
    remap_entry_ctx_t *entry = save_remap_get_map_entry(ctx, offset);
    uint64_t in_pos = offset;
    uint32_t out_pos = 0;
    uint32_t remaining = count;

    while (remaining) {
        uint64_t entry_pos = in_pos - entry->virtual_offset;
        uint32_t bytes_to_read = entry->virtual_offset_end - in_pos < remaining ? (uint32_t)(entry->virtual_offset_end - in_pos) : remaining;

        switch (ctx->type) {
            case STORAGE_BYTES:
                f_lseek(ctx->file, ctx->base_storage_offset + entry->physical_offset + entry_pos);
                f_read(ctx->file, (uint8_t *)buffer + out_pos, bytes_to_read, NULL);
                break;
            case STORAGE_DUPLEX:
                save_duplex_storage_read(ctx->duplex, (uint8_t *)buffer + out_pos, ctx->base_storage_offset + entry->physical_offset + entry_pos, bytes_to_read);
                break;
            default:
                break;
        }

        out_pos += bytes_to_read;
        in_pos += bytes_to_read;
        remaining -= bytes_to_read;

        if (in_pos >= entry->virtual_offset_end)
            entry = entry->next;
    }
    return out_pos;
}

uint32_t save_journal_storage_read(journal_storage_ctx_t *ctx, remap_storage_ctx_t *remap, void *buffer, uint64_t offset, size_t count) {
    uint64_t in_pos = offset;
    uint32_t out_pos = 0;
    uint32_t remaining = count;

    while (remaining) {
        uint32_t block_num = (uint32_t)(in_pos / ctx->block_size);
        uint32_t block_pos = (uint32_t)(in_pos % ctx->block_size);
        uint64_t physical_offset = ctx->map.entries[block_num].physical_index * ctx->block_size + block_pos;
        uint32_t bytes_to_read = ctx->block_size - block_pos < remaining ? ctx->block_size - block_pos : remaining;

        save_remap_read(remap, (uint8_t *)buffer + out_pos, ctx->journal_data_offset + physical_offset, bytes_to_read);

        out_pos += bytes_to_read;
        in_pos += bytes_to_read;
        remaining -= bytes_to_read;
    }
    return out_pos;
}

void save_ivfc_storage_init(hierarchical_integrity_verification_storage_ctx_t *ctx, uint64_t master_hash_offset, ivfc_save_hdr_t *ivfc) {
    ivfc_level_save_ctx_t *levels = ctx->levels;
    levels[0].type = STORAGE_BYTES;
    levels[0].hash_offset = master_hash_offset;
    for (unsigned int i = 1; i < 4; i++) {
        ivfc_level_hdr_t *level = &ivfc->level_headers[i - 1];
        levels[i].type = STORAGE_REMAP;
        levels[i].data_offset = level->logical_offset;
        levels[i].data_size = level->hash_data_size;
    }
    if (ivfc->num_levels == 5) {
        ivfc_level_hdr_t *data_level = &ivfc->level_headers[ivfc->num_levels - 2];
        levels[ivfc->num_levels - 1].type = STORAGE_JOURNAL;
        levels[ivfc->num_levels - 1].data_offset = data_level->logical_offset;
        levels[ivfc->num_levels - 1].data_size = data_level->hash_data_size;
    }

    struct salt_source_t {
        char string[50];
        uint32_t length;
    };

    static struct salt_source_t salt_sources[6] = {
        {"HierarchicalIntegrityVerificationStorage::Master", 48},
        {"HierarchicalIntegrityVerificationStorage::L1", 44},
        {"HierarchicalIntegrityVerificationStorage::L2", 44},
        {"HierarchicalIntegrityVerificationStorage::L3", 44},
        {"HierarchicalIntegrityVerificationStorage::L4", 44},
        {"HierarchicalIntegrityVerificationStorage::L5", 44}
    };
    integrity_verification_info_ctx_t init_info[ivfc->num_levels];

    init_info[0].data = &levels[0];
    init_info[0].block_size = 0;
    for (unsigned int i = 1; i < ivfc->num_levels; i++) {
        init_info[i].data = &levels[i];
        init_info[i].block_size = 1 << ivfc->level_headers[i - 1].block_size;
        se_calc_hmac_sha256(init_info[i].salt, ivfc->salt_source, 0x20, salt_sources[i - 1].string, salt_sources[i - 1].length);
    }

    ctx->integrity_storages[0].next_level = NULL;
    ctx->level_validities = malloc(sizeof(validity_t *) * (ivfc->num_levels - 1));
    for (unsigned int i = 1; i < ivfc->num_levels; i++) {
        integrity_verification_storage_ctx_t *level_data = &ctx->integrity_storages[i - 1];
        level_data->hash_storage = &levels[i - 1];
        level_data->base_storage = &levels[i];
        level_data->sector_size = init_info[i].block_size;
        level_data->_length = init_info[i].data->data_size;
        level_data->sector_count = (level_data->_length + level_data->sector_size - 1) / level_data->sector_size;
        memcpy(level_data->salt, init_info[i].salt, 0x20);
        level_data->block_validities = calloc(1, sizeof(validity_t) * level_data->sector_count);
        ctx->level_validities[i - 1] = level_data->block_validities;
        if (i > 1) {
            level_data->next_level = &ctx->integrity_storages[i - 2];
        }
    }
    ctx->data_level = &levels[ivfc->num_levels - 1];
    ctx->_length = ctx->integrity_storages[ivfc->num_levels - 2]._length;
}

size_t save_ivfc_level_fread(ivfc_level_save_ctx_t *ctx, void *buffer, uint64_t offset, size_t count) {
    switch (ctx->type) {
        case STORAGE_BYTES:
            f_lseek(ctx->save_ctx->file, ctx->hash_offset + offset);
            UINT br = 0;
            f_read(ctx->save_ctx->file, buffer, count, &br);
            return br;
        case STORAGE_REMAP:
            save_remap_read(&ctx->save_ctx->meta_remap_storage, buffer, ctx->data_offset + offset, count);
            return count;
        case STORAGE_JOURNAL:
            save_journal_storage_read(&ctx->save_ctx->journal_storage, &ctx->save_ctx->data_remap_storage, buffer, ctx->data_offset + offset, count);
            return count;
        default:
            return 0;
    }
}

void save_ivfc_storage_read(integrity_verification_storage_ctx_t *ctx, void *buffer, uint64_t offset, size_t count, uint32_t verify) {
    if (count > ctx->sector_size) {
        EPRINTF("IVFC read exceeds sector size!\n");
    }

    uint64_t block_index = offset / ctx->sector_size;

    if (ctx->block_validities[block_index] == VALIDITY_INVALID && verify) {
        EPRINTFARGS("Hash error from previous check\n found at offset %x count %x!\n", (u32)offset, count);
    }

    uint8_t hash_buffer[0x20] = {0};
    uint8_t zeroes[0x20] = {0};
    uint64_t hash_pos = block_index * 0x20;
    if (ctx->next_level) {
        save_ivfc_storage_read(ctx->next_level, hash_buffer, hash_pos, 0x20, verify);
    } else {
        save_ivfc_level_fread(ctx->hash_storage, hash_buffer, hash_pos, 0x20);
    }

    if (!memcmp(hash_buffer, zeroes, 0x20)) {
        memset(buffer, 0, count);
        ctx->block_validities[block_index] = VALIDITY_VALID;
        return;
    }

    save_ivfc_level_fread(ctx->base_storage, buffer, offset, count);

    if (!(verify && ctx->block_validities[block_index] == VALIDITY_UNCHECKED)) {
        return;
    }

    uint8_t hash[0x20] = {0};
    uint8_t *data_buffer = calloc(1, ctx->sector_size + 0x20);
    memcpy(data_buffer, ctx->salt, 0x20);
    memcpy(data_buffer + 0x20, buffer, count);

    se_calc_sha256(hash, data_buffer, ctx->sector_size + 0x20);
    hash[0x1F] |= 0x80;

    free(data_buffer);
    if (memcmp(hash_buffer, hash, 0x20)) {
        ctx->block_validities[block_index] = VALIDITY_INVALID;
    } else {
        ctx->block_validities[block_index] = VALIDITY_VALID;
    }

    if (ctx->block_validities[block_index] == VALIDITY_INVALID && verify) {
        EPRINTFARGS("Hash error from current check\n found at offset %x count %x!\n", (u32)offset, count);
    }
}

uint32_t save_allocation_table_read_entry_with_length(allocation_table_ctx_t *ctx, allocation_table_entry_t *entry) {
    uint32_t length = 1;
    uint32_t entry_index = allocation_table_block_to_entry_index(entry->next);

    allocation_table_entry_t *entries = (allocation_table_entry_t *)((uint8_t *)(ctx->base_storage) + entry_index * SAVE_FAT_ENTRY_SIZE);
    if ((entries[0].next & 0x80000000) == 0) {
        if (entries[0].prev & 0x80000000 && entries[0].prev != 0x80000000) {
            EPRINTF("Invalid range entry in allocation table!\n");
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
    allocation_table_entry_t entry;
    entry.next = block_index;
    uint32_t total_length = 0;
    uint32_t table_size = ctx->header->allocation_table_block_count;
    uint32_t nodes_iterated = 0;

    while (entry.next != 0xFFFFFFFF) {
        total_length += save_allocation_table_read_entry_with_length(ctx, &entry);
        nodes_iterated++;
        if (nodes_iterated > table_size) {
            EPRINTF("Cycle detected in allocation table!\n");
            return 0;
        }
    }
    return total_length;
}

uint64_t save_allocation_table_get_free_space_size(save_filesystem_ctx_t *ctx) {
    uint32_t free_list_start = save_allocation_table_get_free_list_block_index(&ctx->allocation_table);

    if (free_list_start == 0xFFFFFFFF) return 0;

    return ctx->header->block_size * save_allocation_table_get_list_length(&ctx->allocation_table, free_list_start);
}

void save_allocation_table_iterator_begin(allocation_table_iterator_ctx_t *ctx, allocation_table_ctx_t *table, uint32_t initial_block) {
    ctx->fat = table;
    ctx->physical_block = initial_block;
    ctx->virtual_block = 0;

    allocation_table_entry_t entry;
    entry.next = initial_block;
    ctx->current_segment_size = save_allocation_table_read_entry_with_length(ctx->fat, &entry);
    ctx->next_block = entry.next;
    ctx->prev_block = entry.prev;

    if (ctx->prev_block != 0xFFFFFFFF) {
        EPRINTFARGS("Attempted to start FAT iteration from\n invalid block %x!\n", initial_block);
    }
}

int save_allocation_table_iterator_move_next(allocation_table_iterator_ctx_t *ctx) {
    if (ctx->next_block == 0xFFFFFFFF) return 0;

    ctx->virtual_block += ctx->current_segment_size;
    ctx->physical_block = ctx->next_block;

    allocation_table_entry_t entry;
    entry.next = ctx->next_block;
    ctx->current_segment_size = save_allocation_table_read_entry_with_length(ctx->fat, &entry);
    ctx->next_block = entry.next;
    ctx->prev_block = entry.prev;

    return 1;
}

int save_allocation_table_iterator_move_prev(allocation_table_iterator_ctx_t *ctx) {
    if (ctx->prev_block == 0xFFFFFFFF) return 0;

    ctx->physical_block = ctx->prev_block;

    allocation_table_entry_t entry;
    entry.next = ctx->prev_block;
    ctx->current_segment_size = save_allocation_table_read_entry_with_length(ctx->fat, &entry);
    ctx->next_block = entry.next;
    ctx->prev_block = entry.prev;

    ctx->virtual_block -= ctx->current_segment_size;

    return 1;
}

int save_allocation_table_iterator_seek(allocation_table_iterator_ctx_t *ctx, uint32_t block) {
    while (1) {
        if (block < ctx->virtual_block) {
            if (!save_allocation_table_iterator_move_prev(ctx)) return 0;
        } else if (block >= ctx->virtual_block + ctx->current_segment_size) {
            if (!save_allocation_table_iterator_move_next(ctx)) return 0;
        } else {
            return 1;
        }

    }
}

uint32_t save_allocation_table_storage_read(allocation_table_storage_ctx_t *ctx, void *buffer, uint64_t offset, size_t count) {
    allocation_table_iterator_ctx_t iterator;
    save_allocation_table_iterator_begin(&iterator, ctx->fat, ctx->initial_block);
    uint64_t in_pos = offset;
    uint32_t out_pos = 0;
    uint32_t remaining = count;

    while (remaining) {
        uint32_t block_num = (uint32_t)(in_pos / ctx->block_size);
        save_allocation_table_iterator_seek(&iterator, block_num);

        uint32_t segment_pos = (uint32_t)(in_pos - (uint64_t)iterator.virtual_block * ctx->block_size);
        uint64_t physical_offset = iterator.physical_block * ctx->block_size + segment_pos;

        uint32_t remaining_in_segment = iterator.current_segment_size * ctx->block_size - segment_pos;
        uint32_t bytes_to_read = remaining < remaining_in_segment ? remaining : remaining_in_segment;

        uint32_t sector_size = ctx->base_storage->integrity_storages[3].sector_size;
        uint32_t chunk_remaining = bytes_to_read;
        for (unsigned int i = 0; i < bytes_to_read; i += sector_size) {
            uint32_t bytes_to_request = chunk_remaining < sector_size ? chunk_remaining : sector_size;
            save_ivfc_storage_read(&ctx->base_storage->integrity_storages[3], (uint8_t *)buffer + out_pos + i, physical_offset + i, bytes_to_request, ctx->base_storage->data_level->save_ctx->tool_ctx.action & ACTION_VERIFY);
            chunk_remaining -= bytes_to_request;
        }

        out_pos += bytes_to_read;
        in_pos += bytes_to_read;
        remaining -= bytes_to_read;
    }
    return out_pos;
}

uint32_t save_fs_list_get_capacity(save_filesystem_list_ctx_t *ctx) {
    if (!ctx->capacity)
        save_allocation_table_storage_read(&ctx->storage, &ctx->capacity, 4, 4);
    return ctx->capacity;
}

uint32_t save_fs_list_read_entry(save_filesystem_list_ctx_t *ctx, uint32_t index, save_fs_list_entry_t *entry) {
    return save_allocation_table_storage_read(&ctx->storage, entry, index * SAVE_FS_LIST_ENTRY_SIZE, SAVE_FS_LIST_ENTRY_SIZE);
}

int save_fs_list_get_value(save_filesystem_list_ctx_t *ctx, uint32_t index, save_fs_list_entry_t *value) {
    if (index >= save_fs_list_get_capacity(ctx)) {
        return 0;
    }
    save_fs_list_read_entry(ctx, index, value);
    return 1;
}

uint32_t save_fs_get_index_from_key(save_filesystem_list_ctx_t *ctx, save_entry_key_t *key, uint32_t *prev_index) {
    save_fs_list_entry_t entry;
    uint32_t capacity = save_fs_list_get_capacity(ctx);
    save_fs_list_read_entry(ctx, ctx->used_list_head_index, &entry);
    uint32_t prev;
    if (!prev_index) {
        prev_index = &prev;
    }
    *prev_index = ctx->used_list_head_index;
    uint32_t index = entry.next;
    while (index) {
        if (index > capacity) {
            EPRINTFARGS("Save entry index %d out of range!", index);
        }
        save_fs_list_read_entry(ctx, index, &entry);
        if (entry.parent == key->parent && !strcmp(entry.name, key->name)) {
            return index;
        }
        *prev_index = index;
        index = entry.next;
    }
    *prev_index = 0xFFFFFFFF;
    return 0xFFFFFFFF;
}

int save_hierarchical_file_table_find_path_recursive(hierarchical_save_file_table_ctx_t *ctx, save_entry_key_t *key, char *path) {
    key->parent = 0;
    char *pos = strchr(path, '/');
    while (pos) {
        memset(key->name, 0, SAVE_FS_LIST_MAX_NAME_LENGTH);
        char *tmp = strchr(pos, '/');
        if (!tmp) {
            memcpy(key->name, pos, strlen(pos));
            break;
        }
        memcpy(key->name, pos, tmp - pos);
        key->parent = save_fs_get_index_from_key(&ctx->directory_table, key, NULL);
        if (key->parent == 0xFFFFFFFF)
            return 0;
        pos = tmp + 1;
    }
    return 1;
}

int save_hierarchical_file_table_find_next_file(hierarchical_save_file_table_ctx_t *ctx, save_find_position_t *position, save_file_info_t *info, char *name) {
    if (position->next_file == 0) {
        return 0;
    }
    save_fs_list_entry_t entry;
    if(!save_fs_list_get_value(&ctx->file_table, position->next_file, &entry)) {
        return 0;
    }
    position->next_file = entry.value.next_sibling;
    memcpy(name, &entry.name, SAVE_FS_LIST_MAX_NAME_LENGTH);
    memcpy(info, &entry.value.save_file_info, sizeof(save_file_info_t));
    return 1;
}

int save_hierarchical_file_table_find_next_directory(hierarchical_save_file_table_ctx_t *ctx, save_find_position_t *position, char *name) {
    if (position->next_directory == 0) {
        return 0;
    }
    save_fs_list_entry_t entry;
    if(!save_fs_list_get_value(&ctx->directory_table, position->next_directory, &entry)) {
        return 0;
    }
    position->next_directory = entry.value.next_sibling;
    memcpy(name, &entry.name, SAVE_FS_LIST_MAX_NAME_LENGTH);
    return 1;
}

int save_hierarchical_file_table_get_file_entry_by_path(hierarchical_save_file_table_ctx_t *ctx, char *path, save_fs_list_entry_t *entry) {
    save_entry_key_t key;
    if (!save_hierarchical_file_table_find_path_recursive(ctx, &key, path)) {
        EPRINTF("Unable to locate file.");
        return 0;
    }
    u32 index = save_fs_get_index_from_key(&ctx->file_table, &key, NULL);
    if (index == 0xFFFFFFFF) {
        EPRINTF("Unable to get table index for file.");
        return 0;
    }
    if (!save_fs_list_get_value(&ctx->file_table, index, entry)) {
        EPRINTF("Unable to get file entry from index.");
        return 0;
    }
    return 1;
}

void save_open_fat_storage(save_filesystem_ctx_t *ctx, allocation_table_storage_ctx_t *storage_ctx, uint32_t block_index) {
    storage_ctx->base_storage = ctx->base_storage;
    storage_ctx->fat = &ctx->allocation_table;
    storage_ctx->block_size = (uint32_t)ctx->header->block_size;
    storage_ctx->initial_block = block_index;
    storage_ctx->_length = block_index == 0xFFFFFFFF ? 0 : save_allocation_table_get_list_length(storage_ctx->fat, block_index) * storage_ctx->block_size;
}

void save_filesystem_init(save_filesystem_ctx_t *ctx, void *fat, save_fs_header_t *save_fs_header, fat_header_t *fat_header) {
    ctx->allocation_table.base_storage = fat;
    ctx->allocation_table.header = fat_header;
    ctx->allocation_table.free_list_entry_index = 0;
    ctx->header = save_fs_header;

    save_open_fat_storage(ctx, &ctx->file_table.directory_table.storage, fat_header->directory_table_block);
    save_open_fat_storage(ctx, &ctx->file_table.file_table.storage, fat_header->file_table_block);
    ctx->file_table.file_table.free_list_head_index = 0;
    ctx->file_table.file_table.used_list_head_index = 1;
    ctx->file_table.directory_table.free_list_head_index = 0;
    ctx->file_table.directory_table.used_list_head_index = 1;
}

validity_t save_ivfc_validate(hierarchical_integrity_verification_storage_ctx_t *ctx, ivfc_save_hdr_t *ivfc) {
    validity_t result = VALIDITY_VALID;
    for (unsigned int i = 0; i < ivfc->num_levels - 1 && result != VALIDITY_INVALID; i++) {
        integrity_verification_storage_ctx_t *storage = &ctx->integrity_storages[i];

        uint64_t block_size = storage->sector_size;
        uint32_t block_count = (uint32_t)((storage->_length + block_size - 1) / block_size);

        uint8_t *buffer = malloc(block_size);

        for (unsigned int j = 0; j < block_count; j++) {
            if (ctx->level_validities[ivfc->num_levels - 2][j] == VALIDITY_UNCHECKED) {
                uint32_t to_read = storage->_length - block_size * j < block_size ? storage->_length - block_size * j : block_size;
                save_ivfc_storage_read(storage, buffer, block_size * j, to_read, 1);
            }
            if (ctx->level_validities[ivfc->num_levels - 2][j] == VALIDITY_INVALID) {
                result = VALIDITY_INVALID;
                break;
            }
        }
        free(buffer);
    }

    return result;
}

void save_ivfc_set_level_validities(hierarchical_integrity_verification_storage_ctx_t *ctx, ivfc_save_hdr_t *ivfc) {
    for (unsigned int i = 0; i < ivfc->num_levels - 1; i++) {
        validity_t level_validity = VALIDITY_VALID;
        for (unsigned int j = 0; j < ctx->integrity_storages[i].sector_count; j++) {
            if (ctx->level_validities[i][j] == VALIDITY_INVALID) {
                level_validity = VALIDITY_INVALID;
                break;
            }
            if (ctx->level_validities[i][j] == VALIDITY_UNCHECKED && level_validity != VALIDITY_INVALID) {
                level_validity = VALIDITY_UNCHECKED;
            }
        }
        ctx->levels[i].hash_validity = level_validity;
    }
}

validity_t save_filesystem_verify(save_ctx_t *ctx) {
    validity_t journal_validity = save_ivfc_validate(&ctx->core_data_ivfc_storage, &ctx->header.data_ivfc_header);
    save_ivfc_set_level_validities(&ctx->core_data_ivfc_storage, &ctx->header.data_ivfc_header);

    if (!ctx->fat_ivfc_storage.levels[0].save_ctx) return journal_validity;

    validity_t fat_validity = save_ivfc_validate(&ctx->fat_ivfc_storage, &ctx->header.fat_ivfc_header);
    save_ivfc_set_level_validities(&ctx->fat_ivfc_storage, &ctx->header.fat_ivfc_header);

    if (journal_validity != VALIDITY_VALID) return journal_validity;
    if (fat_validity != VALIDITY_VALID) return fat_validity;

    return journal_validity;
}

void save_process(save_ctx_t *ctx) {
    /* Try to parse Header A. */
    f_lseek(ctx->file, 0);
    if (f_read(ctx->file, &ctx->header, sizeof(ctx->header), NULL)) {
        EPRINTF("Failed to read save header!\n");
    }

    save_process_header(ctx);

    if (ctx->header_hash_validity == VALIDITY_INVALID) {
        /* Try to parse Header B. */
        f_lseek(ctx->file, 0x4000);
        if (f_read(ctx->file, &ctx->header, sizeof(ctx->header), NULL)) {
            EPRINTF("Failed to read save header!\n");
        }

        save_process_header(ctx);

        if (ctx->header_hash_validity == VALIDITY_INVALID) {
            EPRINTF("Error: Save header is invalid!\n");
        }
    }

    unsigned char cmac[0x10];
    memset(cmac, 0, 0x10);
    se_aes_key_set(3, ctx->save_mac_key, 0x10);
    se_aes_cmac(3, cmac, 0x10, &ctx->header.layout, sizeof(ctx->header.layout));
    if (memcmp(cmac, &ctx->header.cmac, 0x10) == 0) {
        ctx->header_cmac_validity = VALIDITY_VALID;
    } else {
        ctx->header_cmac_validity = VALIDITY_INVALID;
    }

    /* Initialize remap storages. */
    ctx->data_remap_storage.type = STORAGE_BYTES;
    ctx->data_remap_storage.base_storage_offset = ctx->header.layout.file_map_data_offset;
    ctx->data_remap_storage.header = &ctx->header.main_remap_header;
    ctx->data_remap_storage.map_entries = malloc(sizeof(remap_entry_ctx_t) * ctx->data_remap_storage.header->map_entry_count);
    ctx->data_remap_storage.file = ctx->file;
    f_lseek(ctx->file, ctx->header.layout.file_map_entry_offset);
    for (unsigned int i = 0; i < ctx->data_remap_storage.header->map_entry_count; i++) {
        f_read(ctx->file, &ctx->data_remap_storage.map_entries[i], 0x20, NULL);
        ctx->data_remap_storage.map_entries[i].physical_offset_end = ctx->data_remap_storage.map_entries[i].physical_offset + ctx->data_remap_storage.map_entries[i].size;
        ctx->data_remap_storage.map_entries[i].virtual_offset_end = ctx->data_remap_storage.map_entries[i].virtual_offset + ctx->data_remap_storage.map_entries[i].size;
    }

    /* Initialize data remap storage. */
    ctx->data_remap_storage.segments = save_remap_init_segments(ctx->data_remap_storage.header, ctx->data_remap_storage.map_entries, ctx->data_remap_storage.header->map_entry_count);

    /* Initialize duplex storage. */
    ctx->duplex_layers[0].data_a = (uint8_t *)&ctx->header + ctx->header.layout.duplex_master_offset_a;
    ctx->duplex_layers[0].data_b = (uint8_t *)&ctx->header + ctx->header.layout.duplex_master_offset_b;
    memcpy(&ctx->duplex_layers[0].info, &ctx->header.duplex_header.layers[0], sizeof(duplex_info_t));

    ctx->duplex_layers[1].data_a = malloc(ctx->header.layout.duplex_l1_size);
    save_remap_read(&ctx->data_remap_storage, ctx->duplex_layers[1].data_a, ctx->header.layout.duplex_l1_offset_a, ctx->header.layout.duplex_l1_size);
    ctx->duplex_layers[1].data_b = malloc(ctx->header.layout.duplex_l1_size);
    save_remap_read(&ctx->data_remap_storage, ctx->duplex_layers[1].data_b, ctx->header.layout.duplex_l1_offset_b, ctx->header.layout.duplex_l1_size);
    memcpy(&ctx->duplex_layers[1].info, &ctx->header.duplex_header.layers[1], sizeof(duplex_info_t));

    ctx->duplex_layers[2].data_a = malloc(ctx->header.layout.duplex_data_size);
    save_remap_read(&ctx->data_remap_storage, ctx->duplex_layers[2].data_a, ctx->header.layout.duplex_data_offset_a, ctx->header.layout.duplex_data_size);
    ctx->duplex_layers[2].data_b = malloc(ctx->header.layout.duplex_data_size);
    save_remap_read(&ctx->data_remap_storage, ctx->duplex_layers[2].data_b, ctx->header.layout.duplex_data_offset_b, ctx->header.layout.duplex_data_size);
    memcpy(&ctx->duplex_layers[2].info, &ctx->header.duplex_header.layers[2], sizeof(duplex_info_t));

    /* Initialize hierarchical duplex storage. */
    uint8_t *bitmap = ctx->header.layout.duplex_index == 1 ? ctx->duplex_layers[0].data_b : ctx->duplex_layers[0].data_a;
    save_duplex_storage_init(&ctx->duplex_storage.layers[0], &ctx->duplex_layers[1], bitmap, ctx->header.layout.duplex_master_size);
    ctx->duplex_storage.layers[0]._length = ctx->header.layout.duplex_l1_size;

    bitmap = malloc(ctx->duplex_storage.layers[0]._length);
    save_duplex_storage_read(&ctx->duplex_storage.layers[0], bitmap, 0, ctx->duplex_storage.layers[0]._length);
    save_duplex_storage_init(&ctx->duplex_storage.layers[1], &ctx->duplex_layers[2], bitmap, ctx->duplex_storage.layers[0]._length);
    ctx->duplex_storage.layers[1]._length = ctx->header.layout.duplex_data_size;

    ctx->duplex_storage.data_layer = ctx->duplex_storage.layers[1];

    /* Initialize meta remap storage. */
    ctx->meta_remap_storage.type = STORAGE_DUPLEX;
    ctx->meta_remap_storage.duplex = &ctx->duplex_storage.data_layer;
    ctx->meta_remap_storage.header = &ctx->header.meta_remap_header;
    ctx->meta_remap_storage.map_entries = malloc(sizeof(remap_entry_ctx_t) * ctx->meta_remap_storage.header->map_entry_count);
    ctx->meta_remap_storage.file = ctx->file;
    f_lseek(ctx->file, ctx->header.layout.meta_map_entry_offset);
    for (unsigned int i = 0; i < ctx->meta_remap_storage.header->map_entry_count; i++) {
        f_read(ctx->file, &ctx->meta_remap_storage.map_entries[i], 0x20, NULL);
        ctx->meta_remap_storage.map_entries[i].physical_offset_end = ctx->meta_remap_storage.map_entries[i].physical_offset + ctx->meta_remap_storage.map_entries[i].size;
        ctx->meta_remap_storage.map_entries[i].virtual_offset_end = ctx->meta_remap_storage.map_entries[i].virtual_offset + ctx->meta_remap_storage.map_entries[i].size;
    }

    ctx->meta_remap_storage.segments = save_remap_init_segments(ctx->meta_remap_storage.header, ctx->meta_remap_storage.map_entries, ctx->meta_remap_storage.header->map_entry_count);

   /* Initialize journal map. */
    ctx->journal_map_info.map_storage = malloc(ctx->header.layout.journal_map_table_size);
    save_remap_read(&ctx->meta_remap_storage, ctx->journal_map_info.map_storage, ctx->header.layout.journal_map_table_offset, ctx->header.layout.journal_map_table_size);

    /* Initialize journal storage. */
    ctx->journal_storage.header = &ctx->header.journal_header;
    ctx->journal_storage.journal_data_offset = ctx->header.layout.journal_data_offset;
    ctx->journal_storage._length = ctx->journal_storage.header->total_size - ctx->journal_storage.header->journal_size;
    ctx->journal_storage.file = ctx->file;
    ctx->journal_storage.map.header = &ctx->header.map_header;
    ctx->journal_storage.map.map_storage = ctx->journal_map_info.map_storage;
    ctx->journal_storage.map.entries = malloc(sizeof(journal_map_entry_t) * ctx->journal_storage.map.header->main_data_block_count);
    uint32_t *pos = (uint32_t *)ctx->journal_storage.map.map_storage;
    for (unsigned int i = 0; i < ctx->journal_storage.map.header->main_data_block_count; i++) {
        ctx->journal_storage.map.entries[i].virtual_index = i;
        ctx->journal_storage.map.entries[i].physical_index = *pos & 0x7FFFFFFF;
        pos += 2;
    }
    ctx->journal_storage.block_size = ctx->journal_storage.header->block_size;
    ctx->journal_storage._length = ctx->journal_storage.header->total_size - ctx->journal_storage.header->journal_size;

    /* Initialize core IVFC storage. */
    for (unsigned int i = 0; i < 5; i++) {
        ctx->core_data_ivfc_storage.levels[i].save_ctx = ctx;
    }
    save_ivfc_storage_init(&ctx->core_data_ivfc_storage, ctx->header.layout.ivfc_master_hash_offset_a, &ctx->header.data_ivfc_header);

    /* Initialize FAT storage. */
    if (ctx->header.layout.version < 0x50000) {
        ctx->fat_storage = malloc(ctx->header.layout.fat_size);
        save_remap_read(&ctx->meta_remap_storage, ctx->fat_storage, ctx->header.layout.fat_offset, ctx->header.layout.fat_size);
    } else {
        for (unsigned int i = 0; i < 5; i++) {
            ctx->fat_ivfc_storage.levels[i].save_ctx = ctx;
        }
        save_ivfc_storage_init(&ctx->fat_ivfc_storage, ctx->header.layout.fat_ivfc_master_hash_a, &ctx->header.fat_ivfc_header);
        ctx->fat_storage = malloc(ctx->fat_ivfc_storage._length);
        save_remap_read(&ctx->meta_remap_storage, ctx->fat_storage, ctx->header.fat_ivfc_header.level_headers[ctx->header.fat_ivfc_header.num_levels - 2].logical_offset, ctx->fat_ivfc_storage._length);
    }

    if (ctx->tool_ctx.action & ACTION_VERIFY) {
        save_filesystem_verify(ctx);
    }

    /* Initialize core save filesystem. */
    ctx->save_filesystem_core.base_storage = &ctx->core_data_ivfc_storage;
    save_filesystem_init(&ctx->save_filesystem_core, ctx->fat_storage, &ctx->header.save_header, &ctx->header.fat_header);
}

void save_process_header(save_ctx_t *ctx) {
    if (ctx->header.layout.magic != MAGIC_DISF || ctx->header.duplex_header.magic != MAGIC_DPFS ||
        ctx->header.data_ivfc_header.magic != MAGIC_IVFC || ctx->header.journal_header.magic != MAGIC_JNGL ||
        ctx->header.save_header.magic != MAGIC_SAVE || ctx->header.main_remap_header.magic != MAGIC_RMAP ||
        ctx->header.meta_remap_header.magic != MAGIC_RMAP) {
        EPRINTF("Error: Save header is corrupt!\n");
    }

    ctx->data_ivfc_master = (uint8_t *)&ctx->header + ctx->header.layout.ivfc_master_hash_offset_a;
    ctx->fat_ivfc_master = (uint8_t *)&ctx->header + ctx->header.layout.fat_ivfc_master_hash_a;

    uint8_t hash[0x20];
    se_calc_sha256(hash, &ctx->header.duplex_header, 0x3D00);
    ctx->header_hash_validity = memcmp(hash, ctx->header.layout.hash, 0x20) == 0 ? VALIDITY_VALID : VALIDITY_INVALID;

    ctx->header.data_ivfc_header.num_levels = 5;

    if (ctx->header.layout.version >= 0x50000) {
        ctx->header.fat_ivfc_header.num_levels = 4;
    }
}

void save_free_contexts(save_ctx_t *ctx) {
    for (unsigned int i = 0; i < ctx->data_remap_storage.header->map_segment_count; i++) {
        for (unsigned int j = 0; j < ctx->data_remap_storage.segments[i].entry_count; j++) {
            free(&ctx->data_remap_storage.segments[i].entries[j]);
        }
    }
    free(ctx->data_remap_storage.segments);
    for (unsigned int i = 0; i < ctx->meta_remap_storage.header->map_segment_count; i++) {
        for (unsigned int j = 0; j < ctx->meta_remap_storage.segments[i].entry_count; j++) {
            free(&ctx->meta_remap_storage.segments[i].entries[j]);
        }
    }
    free(ctx->meta_remap_storage.segments);
    free(ctx->data_remap_storage.map_entries);
    free(ctx->meta_remap_storage.map_entries);
    free(ctx->duplex_storage.layers[0].bitmap.bitmap);
    free(ctx->duplex_storage.layers[1].bitmap.bitmap);
    free(ctx->duplex_storage.layers[1].bitmap_storage);
    for (unsigned int i = 1; i < 3; i++) {
        free(ctx->duplex_layers[i].data_a);
        free(ctx->duplex_layers[i].data_b);
    }
    free(ctx->journal_map_info.map_storage);
    free(ctx->journal_storage.map.entries);
    for (unsigned int i = 0; i < ctx->header.data_ivfc_header.num_levels - 1; i++) {
        free(ctx->core_data_ivfc_storage.integrity_storages[i].block_validities);
    }
    free(ctx->core_data_ivfc_storage.level_validities);
    if (ctx->header.layout.version >= 0x50000) {
        for (unsigned int i = 0; i < ctx->header.fat_ivfc_header.num_levels - 1; i++) {
            free(ctx->fat_ivfc_storage.integrity_storages[i].block_validities);
        }
    }
    free(ctx->fat_ivfc_storage.level_validities);
    free(ctx->fat_storage);
}
