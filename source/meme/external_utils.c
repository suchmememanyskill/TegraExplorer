#include "external_utils.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "../gfx/di.h"
#include "../gfx/gfx.h"
#include "../utils/btn.h"
#include "../utils/util.h"
#include "utils.h"
#include "../libs/fatfs/ff.h"
#include "../storage/sdmmc.h"
#include "graphics.h"
#include "../soc/hw_init.h"
#include "../mem/emc.h"
#include "../mem/sdram.h"
#include "../soc/t210.h"
#include "../sec/se.h"
#include "../utils/types.h"
#include "../keys/key_sources.inl"

extern void reloc_patcher(u32 payload_dst, u32 payload_src, u32 payload_size);
extern boot_cfg_t b_cfg;
extern bool sd_mount();
extern void sd_unmount();

int launch_payload(char *path, bool update)
{
	if (!update) gfx_clear_grey(0x1B);
	gfx_con_setpos(0, 0);
	if (!path)
		return 1;

	if (sd_mount()){
		FIL fp;
		if (f_open(&fp, path, FA_READ)){
			EPRINTF("Payload missing!\n");
			return 2;
		}

		void *buf;
		u32 size = f_size(&fp);

		if (size < 0x30000)
			buf = (void *)RCM_PAYLOAD_ADDR;
		else
			buf = (void *)COREBOOT_ADDR;

		if (f_read(&fp, buf, size, NULL)){
			f_close(&fp);

			return 3;
		}

		f_close(&fp);

		sd_unmount();

		if (size < 0x30000){
			reloc_patcher(PATCHED_RELOC_ENTRY, EXT_PAYLOAD_ADDR, ALIGN(size, 0x10));

			reconfig_hw_workaround(false, byte_swap_32(*(u32 *)(buf + size - sizeof(u32))));
		}
		else {
			reloc_patcher(PATCHED_RELOC_ENTRY, EXT_PAYLOAD_ADDR, 0x7000);
			reconfig_hw_workaround(true, 0);
		}

		void (*ext_payload_ptr)() = (void *)EXT_PAYLOAD_ADDR;
		void (*update_ptr)() = (void *)RCM_PAYLOAD_ADDR;

		msleep(100);

		if (!update)
			(*ext_payload_ptr)();
		else {
			EMC(EMC_SCRATCH0) |= EMC_HEKA_UPD;
			(*update_ptr)();
		}
	}

	return 4;
}