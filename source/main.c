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

#include "config/config.h"
#include "config/ini.h"
#include "gfx/di.h"
#include "gfx/gfx.h"
#include "gfx/tui.h"
#include "hos/pkg1.h"
#include "libs/fatfs/ff.h"
#include "mem/heap.h"
#include "mem/minerva.h"
#include "power/max77620.h"
#include "rtc/max77620-rtc.h"
#include "soc/bpmp.h"
#include "soc/hw_init.h"
#include "storage/emummc.h"
#include "storage/nx_emmc.h"
#include "storage/sdmmc.h"
#include "utils/btn.h"
#include "utils/dirlist.h"
#include "utils/sprintf.h"
#include "utils/util.h"
#include "tegraexplorer/te.h"

//#include "keys/keys.h"

sdmmc_t sd_sdmmc;
sdmmc_storage_t sd_storage;
__attribute__ ((aligned (16))) FATFS sd_fs;
static bool sd_mounted;
volatile nyx_storage_t *nyx_str = (nyx_storage_t *)NYX_STORAGE_ADDR;
hekate_config h_cfg;
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
		return res;
	}

	f_write(&fp, buf, size, NULL);
	f_close(&fp);

	return 0;
}

// This is a safe and unused DRAM region for our payloads.
#define RELOC_META_OFF      0x7C
#define PATCHED_RELOC_SZ    0x94
#define PATCHED_RELOC_STACK 0x40007000
#define PATCHED_RELOC_ENTRY 0x40010000
#define EXT_PAYLOAD_ADDR    0xC03C0000
#define RCM_PAYLOAD_ADDR    (EXT_PAYLOAD_ADDR + ALIGN(PATCHED_RELOC_SZ, 0x10))
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

int launch_payload(char *path)
{
	gfx_clear_grey(0x1B);
	gfx_con_setpos(0, 0);
	if (!path)
		return 1;

	if (sd_mount())
	{
		FIL fp;
		if (f_open(&fp, path, FA_READ))
		{
			EPRINTFARGS("Payload file is missing!\n(%s)", path);
			sd_unmount();

			return 1;
		}

		// Read and copy the payload to our chosen address
		void *buf;
		u32 size = f_size(&fp);

		if (size < 0x30000)
			buf = (void *)RCM_PAYLOAD_ADDR;
		else
			buf = (void *)COREBOOT_ADDR;

		if (f_read(&fp, buf, size, NULL))
		{
			f_close(&fp);
			sd_unmount();

			return 1;
		}

		f_close(&fp);

		sd_unmount();

		if (size < 0x30000)
		{
			reloc_patcher(PATCHED_RELOC_ENTRY, EXT_PAYLOAD_ADDR, ALIGN(size, 0x10));

			reconfig_hw_workaround(false, byte_swap_32(*(u32 *)(buf + size - sizeof(u32))));
		}
		else
		{
			reloc_patcher(PATCHED_RELOC_ENTRY, EXT_PAYLOAD_ADDR, 0x7000);
			reconfig_hw_workaround(true, 0);
		}

		// Some cards (Sandisk U1), do not like a fast power cycle. Wait min 100ms.
		sdmmc_storage_init_wait_sd();

		void (*ext_payload_ptr)() = (void *)EXT_PAYLOAD_ADDR;

		// Launch our payload.
		(*ext_payload_ptr)();
	}

	return 1;
}

void launch_tools()
{
	u8 max_entries = 61;
	char *filelist = NULL;
	char *file_sec = NULL;
	char *dir = NULL;

	ment_t *ments = (ment_t *)malloc(sizeof(ment_t) * (max_entries + 3));

	gfx_clear_grey(0x1B);
	gfx_con_setpos(0, 0);

	if (sd_mount())
	{
		dir = (char *)malloc(256);

		memcpy(dir, "sd:/bootloader/payloads", 24);

		filelist = dirlist(dir, NULL, false);

		u32 i = 0;
		u32 i_off = 2;

		if (filelist)
		{
			// Build configuration menu.
			u32 color_idx = 0;

			ments[0].type = MENT_BACK;
			ments[0].caption = "Back";
			ments[0].color = colors[(color_idx++) % 6];
			ments[1].type = MENT_CHGLINE;
			ments[1].color = colors[(color_idx++) % 6];
			if (!f_stat("sd:/atmosphere/reboot_payload.bin", NULL))
			{
				ments[i_off].type = INI_CHOICE;
				ments[i_off].caption = "reboot_payload.bin";
				ments[i_off].color = colors[(color_idx++) % 6];
				ments[i_off].data = "sd:/atmosphere/reboot_payload.bin";
				i_off++;
			}
			if (!f_stat("sd:/ReiNX.bin", NULL))
			{
				ments[i_off].type = INI_CHOICE;
				ments[i_off].caption = "ReiNX.bin";
				ments[i_off].color = colors[(color_idx++) % 6];
				ments[i_off].data = "sd:/ReiNX.bin";
				i_off++;
			}

			while (true)
			{
				if (i > max_entries || !filelist[i * 256])
					break;
				ments[i + i_off].type = INI_CHOICE;
				ments[i + i_off].caption = &filelist[i * 256];
				ments[i + i_off].color = colors[(color_idx++) % 6];
				ments[i + i_off].data = &filelist[i * 256];

				i++;
			}
		}

		if (i > 0)
		{
			memset(&ments[i + i_off], 0, sizeof(ment_t));
			menu_t menu = { ments, "Choose a file to launch", 0, 0 };

			file_sec = (char *)tui_do_menu(&menu);

			if (!file_sec)
			{
				free(ments);
				free(dir);
				free(filelist);
				sd_unmount();
				return;
			}
		}
		else
			EPRINTF("No payloads or modules found.");

		free(ments);
		free(filelist);
	}
	else
	{
		free(ments);
		goto out;
	}

	if (file_sec)
	{
		if (memcmp("sd:/", file_sec, 4)) {
			memcpy(dir + strlen(dir), "/", 2);
			memcpy(dir + strlen(dir), file_sec, strlen(file_sec) + 1);
		}
		else
			memcpy(dir, file_sec, strlen(file_sec) + 1);

		if (launch_payload(dir))
		{
			EPRINTF("Failed to launch payload.");
			free(dir);
		}
	}

out:
	sd_unmount();
	free(dir);

	btn_wait();
}

void dump_sysnand()
{
	h_cfg.emummc_force_disable = true;
	b_cfg.extra_cfg &= ~EXTRA_CFG_DUMP_EMUMMC;
	dump_keys();
}

void dump_emunand()
{
	if (h_cfg.emummc_force_disable)
		return;
	emu_cfg.enabled = 1;
	b_cfg.extra_cfg |= EXTRA_CFG_DUMP_EMUMMC;
	dump_keys();
}

ment_t ment_top[] = {
	MDEF_HANDLER("Dump from SysNAND | Key generation: unk", dump_sysnand, COLOR_RED),
	MDEF_HANDLER("Dump from EmuNAND | Key generation: unk", dump_emunand, COLOR_ORANGE),
	MDEF_CAPTION("---------------", COLOR_YELLOW),
	MDEF_HANDLER("Payloads...", launch_tools, COLOR_GREEN),
	MDEF_CAPTION("---------------", COLOR_BLUE),
	MDEF_HANDLER("Reboot (Normal)", reboot_normal, COLOR_VIOLET),
	MDEF_HANDLER("Reboot (RCM)", reboot_rcm, COLOR_RED),
	MDEF_HANDLER("Power off", power_off, COLOR_ORANGE),
	MDEF_END()
};

menu_t menu_top = { ment_top, NULL, 0, 0 };

void _get_key_generations(char *sysnand_label, char *emunand_label) {
	sdmmc_t sdmmc;
	sdmmc_storage_t storage;
	sdmmc_storage_init_mmc(&storage, &sdmmc, SDMMC_4, SDMMC_BUS_WIDTH_8, 4);
	u8 *pkg1 = (u8 *)malloc(NX_EMMC_BLOCKSIZE);
	sdmmc_storage_set_mmc_partition(&storage, 1);
	sdmmc_storage_read(&storage, 0x100000 / NX_EMMC_BLOCKSIZE, 1, pkg1);
	const pkg1_id_t *pkg1_id = pkg1_identify(pkg1);
	sdmmc_storage_end(&storage);

	if (pkg1_id)
		sprintf(sysnand_label + 36, "% 3d", pkg1_id->kb);
	ment_top[0].caption = sysnand_label;
	if (h_cfg.emummc_force_disable) {
		free(pkg1);
		return;
	}

	emummc_storage_init_mmc(&storage, &sdmmc);
	memset(pkg1, 0, NX_EMMC_BLOCKSIZE);
	emummc_storage_set_mmc_partition(&storage, 1);
	emummc_storage_read(&storage, 0x100000 / NX_EMMC_BLOCKSIZE, 1, pkg1);
	pkg1_id = pkg1_identify(pkg1);
	emummc_storage_end(&storage);

	if (pkg1_id)
		sprintf(emunand_label + 36, "% 3d", pkg1_id->kb);
	free(pkg1);
	ment_top[1].caption = emunand_label;
}

#define IPL_STACK_TOP  0x90010000
#define IPL_HEAP_START 0x90020000

extern void pivot_stack(u32 stack_top);

// todo: chainload to reboot payload or payloads folder option?

void ipl_main()
{
	config_hw();
	pivot_stack(IPL_STACK_TOP);
	heap_init(IPL_HEAP_START);

	set_default_configuration();

	sd_mount();
	minerva_init();
	minerva_change_freq(FREQ_1600);

	display_init();
	u32 *fb = display_init_framebuffer();
	gfx_init_ctxt(fb, 720, 1280, 720);
	gfx_con_init();
	display_backlight_pwm_init();
	display_backlight_brightness(100, 1000);

	bpmp_clk_rate_set(BPMP_CLK_DEFAULT_BOOST);

	/*
	h_cfg.emummc_force_disable = emummc_load_cfg();

	if (b_cfg.boot_cfg & BOOT_CFG_SEPT_RUN)
	{
		if (!(b_cfg.extra_cfg & EXTRA_CFG_DUMP_EMUMMC))
			h_cfg.emummc_force_disable = true;
		dump_keys();
	}

	if (h_cfg.emummc_force_disable)
	{
		ment_top[1].type = MENT_CAPTION;
		ment_top[1].color = 0xFF555555;
		ment_top[1].handler = NULL;
	}

	_get_key_generations((char *)ment_top[0].caption, (char *)ment_top[1].caption);

	while (true)
		tui_do_menu(&menu_top);
	*/

	te_main();

	while (true)
		bpmp_halt();
}
