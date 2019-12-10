/*
 * Copyright (c) 2019 shchmue
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

/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2016        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include <string.h>

#include "../../../common/memory_map.h"

#include "diskio.h"		/* FatFs lower layer API */
#include "../../mem/heap.h"
#include "../../sec/se.h"
#include "../../storage/nx_emmc.h"
#include "../../storage/sdmmc.h"

extern sdmmc_storage_t sd_storage;
extern sdmmc_storage_t storage;
extern emmc_part_t *system_part;

typedef struct {
    u32 sector;
    u32 visit_count;
    u8  tweak[0x10];
    u8  cached_sector[0x200];
    u8  align[8];
} sector_cache_t;

#define MAX_SEC_CACHE_ENTRIES 64
static sector_cache_t *sector_cache = NULL;
static u32 secindex = 0;
bool clear_sector_cache = false;

DSTATUS disk_status (
    BYTE pdrv /* Physical drive number to identify the drive */
)
{
    return 0;
}

DSTATUS disk_initialize (
    BYTE pdrv /* Physical drive number to identify the drive */
)
{
    return 0;
}

static inline void _gf256_mul_x_le(void *block) {
    u8 *pdata = (u8 *)block;
    u32 carry = 0;

    for (u32 i = 0; i < 0x10; i++) {
        u8 b = pdata[i];
        pdata[i] = (b << 1) | carry;
        carry = b >> 7;
    }

    if (carry)
        pdata[0x0] ^= 0x87;
}

static inline int _emmc_xts(u32 ks1, u32 ks2, u32 enc, u8 *tweak, bool regen_tweak, u32 tweak_exp, u64 sec, void *dst, void *src, u32 secsize) {
    int res = 0;
    u8 *pdst = (u8 *)dst;
    u8 *psrc = (u8 *)src;

    if (regen_tweak) {
        for (int i = 0xF; i >= 0; i--) {
            tweak[i] = sec & 0xFF;
            sec >>= 8;
        }
        if (!se_aes_crypt_block_ecb(ks1, 1, tweak, tweak))
            goto out;
    }

    for (u32 i = 0; i < tweak_exp * 0x20; i++)
        _gf256_mul_x_le(tweak);

    u8 temptweak[0x10];
    memcpy(temptweak, tweak, 0x10);

    //We are assuming a 0x10-aligned sector size in this implementation.
    for (u32 i = 0; i < secsize / 0x10; i++) {
        for (u32 j = 0; j < 0x10; j++)
            pdst[j] = psrc[j] ^ tweak[j];
        _gf256_mul_x_le(tweak);
        psrc += 0x10;
        pdst += 0x10;
    }

    se_aes_crypt_ecb(ks2, enc, dst, secsize, src, secsize);

    pdst = (u8 *)dst;

    memcpy(tweak, temptweak, 0x10);
    for (u32 i = 0; i < secsize / 0x10; i++) {
        for (u32 j = 0; j < 0x10; j++)
            pdst[j] = pdst[j] ^ tweak[j];
        _gf256_mul_x_le(tweak);
        pdst += 0x10;
    }


    res = 1;

out:;
    return res;
}

DRESULT disk_read (
    BYTE pdrv,		/* Physical drive number to identify the drive */
    BYTE *buff,		/* Data buffer to store read data */
    DWORD sector,	/* Start sector in LBA */
    UINT count		/* Number of sectors to read */
)
{
    switch (pdrv)
    {
    case 0:
        if (((u32)buff >= DRAM_START) && !((u32)buff % 8))
            return sdmmc_storage_read(&sd_storage, sector, count, buff) ? RES_OK : RES_ERROR;
        u8 *buf = (u8 *)SDMMC_UPPER_BUFFER;
        if (sdmmc_storage_read(&sd_storage, sector, count, buf))
        {
            memcpy(buff, buf, 512 * count);
            return RES_OK;
        }
        return RES_ERROR;

    case 1:;
        __attribute__ ((aligned (16))) static u8 tweak[0x10];
        __attribute__ ((aligned (16))) static u64 prev_cluster = -1;
        __attribute__ ((aligned (16))) static u32 prev_sector = 0;
        bool needs_cache_sector = false;

        if (secindex == 0 || clear_sector_cache) {
            if (!sector_cache)
                sector_cache = (sector_cache_t *)malloc(sizeof(sector_cache_t) * MAX_SEC_CACHE_ENTRIES);
            clear_sector_cache = false;
            secindex = 0;
        }

        u32 s = 0;
        if (count == 1) {
            for ( ; s < secindex; s++) {
                if (sector_cache[s].sector == sector) {
                    sector_cache[s].visit_count++;
                    memcpy(buff, sector_cache[s].cached_sector, 0x200);
                    memcpy(tweak, sector_cache[s].tweak, 0x10);
                    prev_sector = sector;
                    prev_cluster = sector / 0x20;
                    return RES_OK;
                }
            }
            // add to cache
            if (s == secindex && s < MAX_SEC_CACHE_ENTRIES) {
                sector_cache[s].sector = sector;
                sector_cache[s].visit_count++;
                needs_cache_sector = true;
                secindex++;
            }
        }

        if (nx_emmc_part_read(&storage, system_part, sector, count, buff)) {
            u32 tweak_exp = 0;
            bool regen_tweak = true;
            if (prev_cluster != sector / 0x20) { // sector in different cluster than last read
                prev_cluster = sector / 0x20;
                tweak_exp = sector % 0x20;
            } else if (sector > prev_sector) { // sector in same cluster and past last sector
                tweak_exp = sector - prev_sector - 1;
                regen_tweak = false;
            } else { // sector in same cluster and before or same as last sector
                tweak_exp = sector % 0x20;
            }

            // fatfs will never pull more than a cluster
            _emmc_xts(9, 8, 0, tweak, regen_tweak, tweak_exp, prev_cluster, buff, buff, count * 0x200);
            if (needs_cache_sector) {
                memcpy(sector_cache[s].cached_sector, buff, 0x200);
                memcpy(sector_cache[s].tweak, tweak, 0x10);
            }
            prev_sector = sector + count - 1;
            return RES_OK;
        }
        return RES_ERROR;
    }
    return RES_ERROR;
}

DRESULT disk_write (
    BYTE pdrv,			/* Physical drive number to identify the drive */
    const BYTE *buff,	/* Data to be written */
    DWORD sector,		/* Start sector in LBA */
    UINT count			/* Number of sectors to write */
)
{
    if (pdrv == 1)
        return RES_WRPRT;

    if (((u32)buff >= DRAM_START) && !((u32)buff % 8))
        return sdmmc_storage_write(&sd_storage, sector, count, (void *)buff) ? RES_OK : RES_ERROR;
    u8 *buf = (u8 *)SDMMC_UPPER_BUFFER; //TODO: define this somewhere.
    memcpy(buf, buff, 512 * count);
    if (sdmmc_storage_write(&sd_storage, sector, count, buf))
        return RES_OK;
    return RES_ERROR;
}

DRESULT disk_ioctl (
    BYTE pdrv,		/* Physical drive number (0..) */
    BYTE cmd,		/* Control code */
    void *buff		/* Buffer to send/receive control data */
)
{
    return RES_OK;
}
