/*
 * eMMC BIS driver for Nintendo Switch
 *
 * Copyright (c) 2019 shchmue
 * Copyright (c) 2019-2020 CTCaer
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

#include <string.h>

#include <memory_map.h>

#include <mem/heap.h>
#include <sec/se.h>
#include "../storage/nx_emmc.h"
#include "nx_emmc_bis.h"
#include <storage/sdmmc.h>
#include <utils/types.h>

#define MAX_CLUSTER_CACHE_ENTRIES 32768
#define CLUSTER_LOOKUP_EMPTY_ENTRY 0xFFFFFFFF
#define SECTORS_PER_CLUSTER 0x20

typedef struct
{
	u32 cluster_num;                // index of the cluster in the partition
	u32 visit_count;                // used for debugging/access analysis
	u8  dirty;                      // has been modified without writeback flag
	u8  align[7];
	u8  cluster[XTS_CLUSTER_SIZE];  // the cached cluster itself
} cluster_cache_t;

typedef struct
{
	u8 emmc_buffer[XTS_CLUSTER_SIZE];
	cluster_cache_t cluster_cache[];
} bis_cache_t;

static u8 ks_crypt = 0;
static u8 ks_tweak = 0;
static u8 cache_filled = 0;
static u32 dirty_cluster_count = 0;
static u32 cluster_cache_end_index = 0;
static emmc_part_t *system_part = NULL;
static bis_cache_t *bis_cache = (bis_cache_t *)NX_BIS_CACHE_ADDR;
static u32 *cluster_lookup_buf = NULL;
static u32 *cluster_lookup = NULL;
static bool lock_cluster_cache = false;

static void _gf256_mul_x_le(void *block)
{
	u32 *pdata = (u32 *)block;
	u32 carry = 0;

	for (u32 i = 0; i < 4; i++)
	{
		u32 b = pdata[i];
		pdata[i] = (b << 1) | carry;
		carry = b >> 31;
	}

	if (carry)
		pdata[0x0] ^= 0x87;
}

static int _nx_aes_xts_crypt_sec(u32 tweak_ks, u32 crypt_ks, u32 enc, u8 *tweak, bool regen_tweak, u32 tweak_exp, u32 sec, void *dst, const void *src, u32 sec_size)
{
	u32 *pdst = (u32 *)dst;
	u32 *psrc = (u32 *)src;
	u32 *ptweak = (u32 *)tweak;

	if (regen_tweak)
	{
		for (int i = 0xF; i >= 0; i--)
		{
			tweak[i] = sec & 0xFF;
			sec >>= 8;
		}
		if (!se_aes_crypt_block_ecb(tweak_ks, 1, tweak, tweak))
			return 0;
	}

	// tweak_exp allows us to use a saved tweak to reduce _gf256_mul_x_le calls.
	for (u32 i = 0; i < (tweak_exp << 5); i++)
		_gf256_mul_x_le(tweak);

	u8 orig_tweak[0x10] __attribute__((aligned(4)));
	memcpy(orig_tweak, tweak, 0x10);

	// We are assuming a 0x10-aligned sector size in this implementation.
	for (u32 i = 0; i < (sec_size >> 4); i++)
	{
		for (u32 j = 0; j < 4; j++)
			pdst[j] = psrc[j] ^ ptweak[j];

		_gf256_mul_x_le(tweak);
		psrc += 4;
		pdst += 4;
	}

	if (!se_aes_crypt_ecb(crypt_ks, enc, dst, sec_size, dst, sec_size))
		return 0;

	pdst = (u32 *)dst;
	ptweak = (u32 *)orig_tweak;
	for (u32 i = 0; i < (sec_size >> 4); i++)
	{
		for (u32 j = 0; j < 4; j++)
			pdst[j] = pdst[j] ^ ptweak[j];

		_gf256_mul_x_le(orig_tweak);
		pdst += 4;
	}

	return 1;
}

static int nx_emmc_bis_write_block(u32 sector, u32 count, void *buff, bool force_flush)
{
	if (!system_part)
		return 3; // Not ready.

	u8 tweak[0x10] __attribute__((aligned(4)));
	u32 cluster = sector / SECTORS_PER_CLUSTER;
	u32 aligned_sector = cluster * SECTORS_PER_CLUSTER;
	u32 sector_index_in_cluster = sector % SECTORS_PER_CLUSTER;
	u32 cluster_lookup_index = cluster_lookup[cluster];
	bool is_cached = cluster_lookup_index != CLUSTER_LOOKUP_EMPTY_ENTRY;

	// Write to cached cluster.
	if (is_cached)
	{
		if (buff)
			memcpy(bis_cache->cluster_cache[cluster_lookup_index].cluster + sector_index_in_cluster * NX_EMMC_BLOCKSIZE, buff, count * NX_EMMC_BLOCKSIZE);
		else
			buff = bis_cache->cluster_cache[cluster_lookup_index].cluster;
		bis_cache->cluster_cache[cluster_lookup_index].visit_count++;
		if (bis_cache->cluster_cache[cluster_lookup_index].dirty == 0)
			dirty_cluster_count++;
		bis_cache->cluster_cache[cluster_lookup_index].dirty = 1;
		if (!force_flush)
			return 0; // Success.

		// Reset args to trigger a full cluster flush to emmc.
		sector_index_in_cluster = 0;
		sector = aligned_sector;
		count = SECTORS_PER_CLUSTER;
	}

	// Encrypt and write.
	if (!_nx_aes_xts_crypt_sec(ks_tweak, ks_crypt, 1, tweak, true, sector_index_in_cluster, cluster, bis_cache->emmc_buffer, buff, count * NX_EMMC_BLOCKSIZE) ||
		!nx_emmc_part_write(&emmc_storage, system_part, sector, count, bis_cache->emmc_buffer)
	)
		return 1; // R/W error.

	// Mark cache entry not dirty if write succeeds.
	if (is_cached)
	{
		bis_cache->cluster_cache[cluster_lookup_index].dirty = 0;
		dirty_cluster_count--;
	}

	return 0; // Success.
}

static void _nx_emmc_bis_flush_cluster(cluster_cache_t *cache_entry)
{
	nx_emmc_bis_write_block(cache_entry->cluster_num * SECTORS_PER_CLUSTER, SECTORS_PER_CLUSTER, NULL, true);
}

static int nx_emmc_bis_read_block(u32 sector, u32 count, void *buff)
{
	if (!system_part)
		return 3; // Not ready.

	static u32 prev_cluster = -1;
	static u32 prev_sector = 0;
	static u8 tweak[0x10] __attribute__((aligned(4)));
	u8 cache_tweak[0x10] __attribute__((aligned(4)));

	u32 tweak_exp = 0;
	bool regen_tweak = true;

	u32 cluster = sector / SECTORS_PER_CLUSTER;
	u32 aligned_sector = cluster * SECTORS_PER_CLUSTER;
	u32 sector_index_in_cluster = sector % SECTORS_PER_CLUSTER;
	u32 cluster_lookup_index = cluster_lookup[cluster];

	// Read from cached cluster.
	if (cluster_lookup_index != CLUSTER_LOOKUP_EMPTY_ENTRY)
	{
		memcpy(buff, bis_cache->cluster_cache[cluster_lookup_index].cluster + sector_index_in_cluster * NX_EMMC_BLOCKSIZE, count * NX_EMMC_BLOCKSIZE);
		bis_cache->cluster_cache[cluster_lookup_index].visit_count++;
		prev_sector = sector + count - 1;
		prev_cluster = cluster;
		return 0; // Success.
	}

	// Cache cluster.
	if (!lock_cluster_cache)
	{
		// Roll the cache index over and flush if full.
		if (cluster_cache_end_index >= MAX_CLUSTER_CACHE_ENTRIES)
		{
			cluster_cache_end_index = 0;
			cache_filled = 1;
		}
		// Check if cache entry was previously in use in case of cache loop.
		if (cache_filled == 1 && bis_cache->cluster_cache[cluster_cache_end_index].dirty == 1)
			_nx_emmc_bis_flush_cluster(&bis_cache->cluster_cache[cluster_cache_end_index]);

		if (cache_filled == 1)
			cluster_lookup[bis_cache->cluster_cache[cluster_cache_end_index].cluster_num] = -1;

		bis_cache->cluster_cache[cluster_cache_end_index].cluster_num = cluster;
		bis_cache->cluster_cache[cluster_cache_end_index].visit_count = 1;
		bis_cache->cluster_cache[cluster_cache_end_index].dirty = 0;
		cluster_lookup[cluster] = cluster_cache_end_index;

		// Read and decrypt the whole cluster the sector resides in.
		if (!nx_emmc_part_read(&emmc_storage, system_part, aligned_sector, SECTORS_PER_CLUSTER, bis_cache->emmc_buffer) ||
			!_nx_aes_xts_crypt_sec(ks_tweak, ks_crypt, 0, cache_tweak, true, 0, cluster, bis_cache->emmc_buffer, bis_cache->emmc_buffer, XTS_CLUSTER_SIZE)
		)
			return 1; // R/W error.

		// Copy to cluster cache.
		memcpy(bis_cache->cluster_cache[cluster_cache_end_index].cluster, bis_cache->emmc_buffer, XTS_CLUSTER_SIZE);
		memcpy(buff, bis_cache->emmc_buffer + sector_index_in_cluster * NX_EMMC_BLOCKSIZE, count * NX_EMMC_BLOCKSIZE);
		cluster_cache_end_index++;
		return 0; // Success.
	}

	// If not reading from or writing to cache, do a regular read and decrypt.
	if (!nx_emmc_part_read(&emmc_storage, system_part, sector, count, bis_cache->emmc_buffer))
		return 1; // R/W error.

	if (prev_cluster != cluster) // Sector in different cluster than last read.
	{
		prev_cluster = cluster;
		tweak_exp = sector_index_in_cluster;
	}
	else if (sector > prev_sector) // Sector in same cluster and past last sector.
	{
		// Calculates the new tweak using the saved one, reducing expensive _gf256_mul_x_le calls.
		tweak_exp = sector - prev_sector - 1;
		regen_tweak = false;
	}
	else // Sector in same cluster and before or same as last sector.
		tweak_exp = sector_index_in_cluster;

	// Maximum one cluster (1 XTS crypto block 16KB).
	if (!_nx_aes_xts_crypt_sec(ks_tweak, ks_crypt, 0, tweak, regen_tweak, tweak_exp, prev_cluster, buff, bis_cache->emmc_buffer, count * NX_EMMC_BLOCKSIZE))
		return 1; // R/W error.
	prev_sector = sector + count - 1;

	return 0; // Success.
}

int nx_emmc_bis_read(u32 sector, u32 count, void *buff)
{
	int res = 1;
	u8 *buf = (u8 *)buff;
	u32 curr_sct = sector;

	while (count)
	{
		u32 sct_cnt = MIN(count, 0x20);
		res = nx_emmc_bis_read_block(curr_sct, sct_cnt, buf);
		if (res)
			return 1;

		count -= sct_cnt;
		curr_sct += sct_cnt;
		buf += NX_EMMC_BLOCKSIZE * sct_cnt;
	}

	return res;
}

int nx_emmc_bis_write(u32 sector, u32 count, void *buff)
{
	int res = 1;
	u8 *buf = (u8 *)buff;
	u32 curr_sct = sector;

	while (count)
	{
		u32 sct_cnt = MIN(count, 0x20);
		res = nx_emmc_bis_write_block(curr_sct, sct_cnt, buf, false);
		if (res)
			return 1;

		count -= sct_cnt;
		curr_sct += sct_cnt;
		buf += NX_EMMC_BLOCKSIZE * sct_cnt;
	}

	nx_emmc_bis_finalize();
	return res;
}

void nx_emmc_bis_cluster_cache_init()
{
	u32 cluster_lookup_size = (system_part->lba_end - system_part->lba_start + 1) / SECTORS_PER_CLUSTER * sizeof(*cluster_lookup);

	if (cluster_lookup_buf)
		free(cluster_lookup_buf);

	// Check if carveout protected, in case of old hwinit (pre 4.0.0) chainload.
	*(vu32 *)NX_BIS_LOOKUP_ADDR = 0;
	if (*(vu32 *)NX_BIS_LOOKUP_ADDR != 0)
	{
		cluster_lookup_buf = (u32 *)malloc(cluster_lookup_size + 0x2000);
		cluster_lookup = (u32 *)ALIGN((u32)cluster_lookup_buf, 0x1000);
	}
	else
	{
		cluster_lookup_buf = NULL;
		cluster_lookup = (u32 *)NX_BIS_LOOKUP_ADDR;
	}

	// Clear cluster lookup table and reset end index.
	memset(cluster_lookup, -1, cluster_lookup_size);
	cluster_cache_end_index = 0;
	lock_cluster_cache = false;

	dirty_cluster_count = 0;
	cache_filled = 0;
}

void nx_emmc_bis_init(emmc_part_t *part)
{
	system_part = part;

	nx_emmc_bis_cluster_cache_init();

	switch (part->index)
	{
	case 0:  // PRODINFO.
	case 1:  // PRODINFOF.
		ks_crypt = 0;
		ks_tweak = 1;
		break;
	case 8:  // SAFE.
		ks_crypt = 2;
		ks_tweak = 3;
		break;
	case 9:  // SYSTEM.
	case 10: // USER.
		ks_crypt = 4;
		ks_tweak = 5;
		break;
	}
}

void nx_emmc_bis_finalize()
{
	if (dirty_cluster_count == 0)
		return;

	u32 limit = cache_filled == 1 ? MAX_CLUSTER_CACHE_ENTRIES : cluster_cache_end_index;
	u32 clusters_to_flush = dirty_cluster_count;
	for (u32 i = 0; i < limit && clusters_to_flush; i++)
	{
		if (bis_cache->cluster_cache[i].dirty) {
			_nx_emmc_bis_flush_cluster(&bis_cache->cluster_cache[i]);
			clusters_to_flush--;
		}
	}
}

// Set cluster cache lock according to arg.
void nx_emmc_bis_cache_lock(bool lock)
{
	lock_cluster_cache = lock;
}
