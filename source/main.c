/*
 * Copyright (c) 2018 naehrwert
 *
 * Copyright (c) 2018-2020 CTCaer
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

#include <string.h>

#include "config.h"
#include <gfx/di.h>
#include <gfx_utils.h>
#include <libs/fatfs/ff.h>
#include <mem/heap.h>
#include <mem/minerva.h>
#include <power/bq24193.h>
#include <power/max17050.h>
#include <power/max77620.h>
#include <rtc/max77620-rtc.h>
#include <soc/bpmp.h>
#include <soc/hw_init.h>
#include "storage/emummc.h"
#include "storage/nx_emmc.h"
#include <storage/nx_sd.h>
#include <storage/sdmmc.h>
#include <utils/btn.h>
#include <utils/dirlist.h>
#include <utils/ini.h>
#include <utils/sprintf.h>
#include <utils/util.h>
#include "hid/hid.h"
#include <input/joycon.h>
#include "gfx/menu.h"
#include "utils/vector.h"
#include "gfx/gfxutils.h"
#include "tegraexplorer/mainmenu.h"
#include "tegraexplorer/tconf.h"
#include "err.h"
#include <soc/pmc.h>
#include "keys/keys.h"
#include "keys/keyfile.h"
#include "storage/mountmanager.h"


hekate_config h_cfg;
boot_cfg_t __attribute__((section ("._boot_cfg"))) b_cfg;

volatile nyx_storage_t *nyx_str = (nyx_storage_t *)NYX_STORAGE_ADDR;

// This is a safe and unused DRAM region for our payloads.
#define RELOC_META_OFF      0x7C
#define PATCHED_RELOC_SZ    0x94
#define PATCHED_RELOC_STACK 0x40007000
#define PATCHED_RELOC_ENTRY 0x40010000
#define EXT_PAYLOAD_ADDR    0xC0000000
#define RCM_PAYLOAD_ADDR    (EXT_PAYLOAD_ADDR + ALIGN(PATCHED_RELOC_SZ, 0x10))
#define COREBOOT_END_ADDR   0xD0000000
#define CBFS_DRAM_EN_ADDR   0x4003e000
#define  CBFS_DRAM_MAGIC    0x4452414D // "DRAM"

static void *coreboot_addr;

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
		memcpy((u8 *)(payload_src + ALIGN(PATCHED_RELOC_SZ, 0x10)), coreboot_addr, 0x7000); //Bootblock
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
		{
			coreboot_addr = (void *)(COREBOOT_END_ADDR - size);
			buf = coreboot_addr;
		}

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

			hw_reinit_workaround(false, byte_swap_32(*(u32 *)(buf + size - sizeof(u32))));
		}
		else
		{
			reloc_patcher(PATCHED_RELOC_ENTRY, EXT_PAYLOAD_ADDR, 0x7000);
			hw_reinit_workaround(true, 0);
		}

		// Some cards (Sandisk U1), do not like a fast power cycle. Wait min 100ms.
		sdmmc_storage_init_wait_sd();

		void (*ext_payload_ptr)() = (void *)EXT_PAYLOAD_ADDR;

		// Launch our payload.
		(*ext_payload_ptr)();
	}

	return 1;
}

extern void pivot_stack(u32 stack_top);

#define EXCP_EN_ADDR   0x4003FFFC
#define  EXCP_MAGIC 0x30505645      // EVP0
#define EXCP_TYPE_ADDR 0x4003FFF8
#define  EXCP_TYPE_RESET 0x545352   // RST
#define  EXCP_TYPE_UNDEF 0x464455   // UDF
#define  EXCP_TYPE_PABRT 0x54424150 // PABT
#define  EXCP_TYPE_DABRT 0x54424144 // DABT
#define EXCP_LR_ADDR   0x4003FFF4

static inline void _show_errors()
{
	u32 *excp_enabled = (u32 *)EXCP_EN_ADDR;
	u32 *excp_type = (u32 *)EXCP_TYPE_ADDR;
	u32 *excp_lr = (u32 *)EXCP_LR_ADDR;

	if (*excp_enabled == EXCP_MAGIC)
		h_cfg.errors |= ERR_EXCEPTION;

	if (h_cfg.errors)
	{
		

		/*
		if (h_cfg.errors & ERR_SD_BOOT_EN)
			WPRINTF("Failed to mount SD!\n");

		if (h_cfg.errors & ERR_LIBSYS_LP0)
			WPRINTF("Missing LP0 (sleep mode) lib!\n");
		if (h_cfg.errors & ERR_LIBSYS_MTC)
			WPRINTF("Missing or old Minerva lib!\n");

		if (h_cfg.errors & (ERR_LIBSYS_LP0 | ERR_LIBSYS_MTC))
			WPRINTF("\nUpdate bootloader folder!\n\n");
		*/

		if (h_cfg.errors & ERR_EXCEPTION)
		{
			gfx_clearscreen();
			WPRINTFARGS("LR %08X", *excp_lr);
			u32 exception = 0;

			switch (*excp_type)
			{
			case EXCP_TYPE_RESET:
				exception = TE_EXCEPTION_RESET;
				break;
			case EXCP_TYPE_UNDEF:
				exception = TE_EXCEPTION_UNDEFINED;
				break;
			case EXCP_TYPE_PABRT:
				exception = TE_EXCEPTION_PREF_ABORT;
				break;
			case EXCP_TYPE_DABRT:
				exception = TE_EXCEPTION_DATA_ABORT;
				break;
			}

			// Clear the exception.
			*excp_enabled = 0;
			DrawError(newErrCode(exception));
		}
	}
}

void ipl_main()
{
	// Do initial HW configuration. This is compatible with consecutive reruns without a reset.
	hw_init();

	// Pivot the stack so we have enough space.
	pivot_stack(IPL_STACK_TOP);

	// Tegra/Horizon configuration goes to 0x80000000+, package2 goes to 0xA9800000, we place our heap in between.
	heap_init(IPL_HEAP_START);

#ifdef DEBUG_UART_PORT
	uart_send(DEBUG_UART_PORT, (u8 *)"hekate: Hello!\r\n", 16);
	uart_wait_idle(DEBUG_UART_PORT, UART_TX_IDLE);
#endif

	// Set bootloader's default configuration.
	set_default_configuration();

	// Mount SD Card.
	h_cfg.errors |= !sd_mount() ? ERR_SD_BOOT_EN : 0;

	TConf.minervaEnabled = !minerva_init();
	TConf.FSBuffSize = (TConf.minervaEnabled) ? 0x800000 : 0x10000;

	// Train DRAM and switch to max frequency.
	if (TConf.minervaEnabled) //!TODO: Add Tegra210B01 support to minerva.
		h_cfg.errors |= ERR_LIBSYS_MTC;
	minerva_change_freq(FREQ_1600);

	display_init();

	u32 *fb = display_init_framebuffer_pitch();
	gfx_init_ctxt(fb, 720, 1280, 720);

	gfx_con_init();

	display_backlight_pwm_init();
	display_backlight_brightness(100, 1000);

	// Overclock BPMP.
	bpmp_clk_rate_set(BPMP_CLK_DEFAULT_BOOST);

	emummc_load_cfg();
	// Ignore whether emummc is enabled.
	h_cfg.emummc_force_disable = emu_cfg.sector == 0 && !emu_cfg.path;
	emu_cfg.enabled = !h_cfg.emummc_force_disable;
	h_cfg.emummc_force_disable = 1;

	TConf.pkg1ID = "Unk";

	hidInit();

	//gfx_clearscreen();
	//Vector_t a = vecFromArray(testEntries, 9, sizeof(MenuEntry_t));
	//u32 res = newMenu(&a, 0, 40, 5, testAdd, NULL);

	//gfx_clearscreen();
	//DrawError(newErrCode(1));

	// TODO: Write exceptions in err.c and check them here

	_show_errors();

	gfx_clearscreen();

	int res = -1;

	
	if (btn_read() & BTN_VOL_DOWN || DumpKeys())
		res = GetKeysFromFile("sd:/switch/prod.keys");

	TConf.keysDumped = (res > 0) ? 0 : 1;

	if (res > 0)
		DrawError(newErrCode(TE_ERR_KEYDUMP_FAIL));
	
	if (TConf.keysDumped)
		SetKeySlots();
	
	if (res == 0)
		hidWait();
	EnterMainMenu();

	// Halt BPMP if we managed to get out of execution.
	while (true)
		bpmp_halt();
}
