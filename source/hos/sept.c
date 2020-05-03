/*
 * Copyright (c) 2019 CTCaer
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

#include "sept.h"
#include "../gfx/di.h"
#include "../libs/fatfs/ff.h"
#include "../mem/heap.h"
#include "../soc/hw_init.h"
#include "../soc/pmc.h"
#include "../soc/t210.h"
#include "../storage/nx_emmc.h"
#include "../storage/sdmmc.h"
#include "../utils/btn.h"
#include "../utils/types.h"

#include "../gfx/gfx.h"

#define PATCHED_RELOC_SZ 0x94

#define WB_RST_ADDR 0x40010ED0
#define WB_RST_SIZE 0x30

u8 warmboot_reboot[] = {
	0x14, 0x00, 0x9F, 0xE5, // LDR R0, =0x7000E450
	0x01, 0x10, 0xB0, 0xE3, // MOVS R1, #1
	0x00, 0x10, 0x80, 0xE5, // STR R1, [R0]
	0x0C, 0x00, 0x9F, 0xE5, // LDR R0, =0x7000E400
	0x10, 0x10, 0xB0, 0xE3, // MOVS R1, #0x10
	0x00, 0x10, 0x80, 0xE5, // STR R1, [R0]
	0xFE, 0xFF, 0xFF, 0xEA, // LOOP
	0x50, 0xE4, 0x00, 0x70, // #0x7000E450
	0x00, 0xE4, 0x00, 0x70  // #0x7000E400
};

#define SEPT_PRI_ADDR   0x4003F000

#define SEPT_PK1T_ADDR  0xC0400000
#define SEPT_PK1T_STACK 0x40008000
#define SEPT_TCSZ_ADDR  (SEPT_PK1T_ADDR - 0x4)
#define SEPT_STG1_ADDR  (SEPT_PK1T_ADDR + 0x2E100)
#define SEPT_STG2_ADDR  (SEPT_PK1T_ADDR + 0x60E0)
#define SEPT_PKG_SZ     (0x2F100 + WB_RST_SIZE)

extern u32 color_idx;
extern boot_cfg_t b_cfg;
extern void sd_unmount();
extern void reloc_patcher(u32 payload_dst, u32 payload_src, u32 payload_size);

int reboot_to_sept(const u8 *tsec_fw, const u32 tsec_size, const u32 kb)
{
	FIL fp;

	// Copy warmboot reboot code and TSEC fw.
	memcpy((u8 *)(SEPT_PK1T_ADDR - WB_RST_SIZE), (u8 *)warmboot_reboot, sizeof(warmboot_reboot));
	memcpy((void *)SEPT_PK1T_ADDR, tsec_fw, tsec_size);
	*(vu32 *)SEPT_TCSZ_ADDR = tsec_size;

	// Copy sept-primary.
	if (f_open(&fp, "sd:/sept/sept-primary.bin", FA_READ))
		goto error;

	if (f_read(&fp, (u8 *)SEPT_STG1_ADDR, f_size(&fp), NULL))
	{
		f_close(&fp);
		goto error;
	}
	f_close(&fp);

	// Copy sept-secondary.
	if (kb < KB_FIRMWARE_VERSION_810)
	{
		if (f_open(&fp, "sd:/sept/sept-secondary_00.enc", FA_READ))
			if (f_open(&fp, "sd:/sept/sept-secondary.enc", FA_READ)) // Try the deprecated version.
				goto error;
	}
	else
	{
		if (f_open(&fp, "sd:/sept/sept-secondary_01.enc", FA_READ))
			goto error;
	}

	if (f_read(&fp, (u8 *)SEPT_STG2_ADDR, f_size(&fp), NULL))
	{
		f_close(&fp);
		goto error;
	}
	f_close(&fp);

	// Save auto boot config to sept payload, if any.
	boot_cfg_t *tmp_cfg = malloc(sizeof(boot_cfg_t));
	memcpy(tmp_cfg, &b_cfg, sizeof(boot_cfg_t));

	tmp_cfg->boot_cfg |= BOOT_CFG_SEPT_RUN;

	if (f_open(&fp, "sd:/sept/payload.bin", FA_READ | FA_WRITE)) {
		free(tmp_cfg);
		goto error;
	}

	f_lseek(&fp, PATCHED_RELOC_SZ);
	f_write(&fp, tmp_cfg, sizeof(boot_cfg_t), NULL);

	f_close(&fp);

	sd_unmount();
	gfx_printf("\n%kPress Power or Vol +/-\n   to Reboot to Sept...", colors[(color_idx++) % 6]);

	u32 pk1t_sept = SEPT_PK1T_ADDR - (ALIGN(PATCHED_RELOC_SZ, 0x10) + WB_RST_SIZE);

	void (*sept)() = (void *)pk1t_sept;

	reloc_patcher(WB_RST_ADDR, pk1t_sept, SEPT_PKG_SZ);

	// Patch SDRAM init to perform an SVC immediately after second write.
	PMC(APBDEV_PMC_SCRATCH45) = 0x2E38DFFF;
	PMC(APBDEV_PMC_SCRATCH46) = 0x6001DC28;
	// Set SVC handler to jump to sept-primary in IRAM.
	PMC(APBDEV_PMC_SCRATCH33) = SEPT_PRI_ADDR;
	PMC(APBDEV_PMC_SCRATCH40) = 0x6000F208;

	reconfig_hw_workaround(false, 0);

	(*sept)();

error:
	EPRINTF("\nSept files not found in sd:/sept!\nPlace appropriate files and try again.");
	display_backlight_brightness(100, 1000);

	btn_wait();

	return 0;
}