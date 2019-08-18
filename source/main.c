/*
 * Copyright (c) 2018 naehrwert
 *
 * Copyright (c) 2018-2019 CTCaer
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

#include "gfx/di.h"
#include "gfx/gfx.h"
#include "libs/fatfs/ff.h"
#include "mem/heap.h"
#include "power/max77620.h"
#include "rtc/max77620-rtc.h"
#include "soc/hw_init.h"
#include "storage/sdmmc.h"
#include "utils/util.h"
#include "utils/btn.h"
#include "meme/main.h"
#include "meme/utils.h"

sdmmc_t sd_sdmmc;
sdmmc_storage_t sd_storage;
__attribute__ ((aligned (16))) FATFS sd_fs;
static bool sd_mounted;

boot_cfg_t __attribute__((section ("._boot_cfg"))) b_cfg;

bool sd_mount()
{
    if (sd_mounted)
        return true;

    if (!sdmmc_storage_init_sd(&sd_storage, &sd_sdmmc, SDMMC_1, SDMMC_BUS_WIDTH_4, 11))
    {
        EPRINTF("Failed to init SD card.\nMake sure that it is inserted.\nOr that SD reader is properly seated!");
    }
    else
    {
        int res = 0;
        res = f_mount(&sd_fs, "sd:", 1);
        if (res == FR_OK)
        {
            sd_mounted = 1;
            return true;
        }
        else
        {
            EPRINTFARGS("Failed to mount SD card (FatFS Error %d).\nMake sure that a FAT partition exists..", res);
        }
    }

    return false;
}

void sd_unmount()
{
    if (sd_mounted)
    {
        f_mount(NULL, "sd:", 1);
        sdmmc_storage_end(&sd_storage);
        sd_mounted = false;
    }
}

void *sd_file_read(const char *path, u32 *fsize)
{
	FIL fp;
	if (f_open(&fp, path, FA_READ) != FR_OK)
		return NULL;

	u32 size = f_size(&fp);
	if (fsize)
		*fsize = size;

	void *buf = malloc(size);

	if (f_read(&fp, buf, size, NULL) != FR_OK)
	{
		free(buf);
		f_close(&fp);

		return NULL;
	}

	f_close(&fp);

	return buf;
}

int sd_save_to_file(void *buf, u32 size, const char *filename)
{
    FIL fp;
    u32 res = 0;
    res = f_open(&fp, filename, FA_CREATE_ALWAYS | FA_WRITE);
    if (res)
    {
        EPRINTFARGS("Error (%d) creating file\n%s.\n", res, filename);
        return 1;
    }

    f_write(&fp, buf, size, NULL);
    f_close(&fp);

    return 0;
}

// This is a safe and unused DRAM region for our payloads.
#define RELOC_META_OFF      0x7C
#define PATCHED_RELOC_SZ    0x94
#define PATCHED_RELOC_STACK 0x40007000
#define COREBOOT_ADDR       (0xD0000000 - 0x100000)
#define CBFS_DRAM_EN_ADDR   0x4003e000
#define  CBFS_DRAM_MAGIC    0x4452414D // "DRAM"


void reloc_patcher(u32 payload_dst, u32 payload_src, u32 payload_size)
{
    memcpy((u8 *)payload_src, (u8 *)IPL_LOAD_ADDR, PATCHED_RELOC_SZ);

    volatile reloc_meta_t *relocator = (reloc_meta_t *)(payload_src + RELOC_META_OFF);

    relocator->start = payload_dst - ALIGN(PATCHED_RELOC_SZ, 0x10);
    relocator->stack = PATCHED_RELOC_STACK;
    relocator->end   = payload_dst + payload_size;
    relocator->ep    = payload_dst;

    if (payload_size == 0x7000)
    {
        memcpy((u8 *)(payload_src + ALIGN(PATCHED_RELOC_SZ, 0x10)), (u8 *)COREBOOT_ADDR, 0x7000); //Bootblock
        *(vu32 *)CBFS_DRAM_EN_ADDR = CBFS_DRAM_MAGIC;
    }
}

#define IPL_STACK_TOP  0x4003F000
#define IPL_HEAP_START 0x90020000

extern void pivot_stack(u32 stack_top);

void ipl_main()
{
    config_hw();
    pivot_stack(IPL_STACK_TOP);
    heap_init(IPL_HEAP_START);

    display_init();
    u32 *fb = display_init_framebuffer();
    gfx_init_ctxt(fb, 720, 1280, 720);
    gfx_con_init();
    display_backlight_pwm_init();

    meme_main();
    sd_unmount();
}
