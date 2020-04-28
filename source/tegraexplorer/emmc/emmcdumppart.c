#include <string.h>
#include "emmcoperations.h"
#include "../../gfx/gfx.h"
#include "../gfx/gfxutils.h"
#include "../../utils/types.h"
#include "emmc.h"
#include "../../storage/emummc.h"
#include "../common/common.h"
#include "../../libs/fatfs/ff.h"
#include "../../utils/sprintf.h"
#include "../../utils/btn.h"
#include "../../mem/heap.h"
#include "../../storage/nx_emmc.h"
#include "../common/types.h"
#include "../utils/utils.h"
#include "../fs/fsutils.h"

extern sdmmc_storage_t storage;
extern emmc_part_t *system_part;

int emmcDumpPart(char *path, sdmmc_storage_t *mmcstorage, emmc_part_t *part){
	FIL fp;
	u8 *buf;
	u32 lba_curr = part->lba_start;
	u32 bytesWritten = 0;
	u32 totalSectors = part->lba_end - part->lba_start + 1;
	u64 totalSize = (u64)((u64)totalSectors << 9);
	u32 num = 0;
	u32 pct = 0;
	int res;

	gfx_printf("Initializing\r");
	buf = calloc(BUFSIZE, sizeof(u8));

	if (!buf){
		gfx_errDisplay("dump_emmc_part", ERR_MEM_ALLOC_FAILED, 1);
		return -1;
	}

	if ((res = f_open(&fp, path, FA_CREATE_ALWAYS | FA_WRITE))){
		gfx_errDisplay("dump_emmc_part", res, 2);
		return -1;
	}

	f_lseek(&fp, totalSize);
	f_lseek(&fp, 0);

	while (totalSectors > 0){
		num = MIN(totalSectors, 64);
		if (!emummc_storage_read(mmcstorage, lba_curr, num, buf)){
			gfx_errDisplay("dump_emmc_part", ERR_EMMC_READ_FAILED, 3);
			return -1;
		}
		if ((res = f_write(&fp, buf, num * NX_EMMC_BLOCKSIZE, NULL))){
			gfx_errDisplay("dump_emmc_part", res, 4);
			return -1;
		}
		pct = (u64)((u64)(lba_curr - part->lba_start) * 100u) / (u64)(part->lba_end - part->lba_start);
		gfx_printf("Progress: %d%%\r", pct);

		lba_curr += num;
		totalSectors -= num;
		bytesWritten += num * NX_EMMC_BLOCKSIZE;
	}
	gfx_printf("                \r");
	f_close(&fp);
	free(buf);
	return 0;
}

int existsCheck(char *path){
	int res = 0;

	if (fsutil_checkfile(path)){
		gfx_printf("File already exists! Overwrite?\nVol +/- to cancel\n");
		res = gfx_makewaitmenu("Power to continue", 3);
		gfx_printf("\r                 \r");
		return res;
	}

	return 1;
}

/*
int dump_emmc_parts(u16 parts, u8 mmctype){
	char *path;
	char basepath[] = "sd:/tegraexplorer/partition_dumps";
	f_mkdir("sd:/tegraexplorer");
	f_mkdir("sd:/tegraexplorer/partition_dumps");

	connect_mmc(mmctype);
	gfx_clearscreen();

	if (parts & PART_BOOT){
		emmc_part_t bootPart;
		const u32 BOOT_PART_SIZE = storage.ext_csd.boot_mult << 17;
		memset(&bootPart, 0, sizeof(bootPart));
		
		bootPart.lba_start = 0;
		bootPart.lba_end = (BOOT_PART_SIZE / NX_EMMC_BLOCKSIZE) - 1;
		
		for (int i = 0; i < 2; i++){
			strcpy(bootPart.name, "BOOT");
			bootPart.name[4] = (u8)('0' + i);
			bootPart.name[5] = 0;     

			emummc_storage_set_mmc_partition(&storage, i + 1);
			utils_copystring(fsutil_getnextloc(basepath, bootPart.name), &path);

			if (!existsCheck(path))
				continue;

			gfx_printf("Dumping %s\n", bootPart.name);
 
			dump_emmc_part(path, &storage, &bootPart);
			free(path);
		}
		emummc_storage_set_mmc_partition(&storage, 0);
	}

	if (parts & PART_PKG2){
		for (int i = 0; i < 6; i++){
			utils_copystring(fsutil_getnextloc(basepath, pkg2names[i]), &path);
			gfx_printf("Dumping %s\n", pkg2names[i]);

			emmcDumpSpecific(pkg2names[i], path);
			free(path);
		}
	}

	gfx_printf("\nDone!");
	btn_wait();
	return 0;
}
*/

int emmcDumpBoot(char *basePath){
	emmc_part_t bootPart;
	const u32 BOOT_PART_SIZE = storage.ext_csd.boot_mult << 17;
	char *path;
	memset(&bootPart, 0, sizeof(emmc_part_t));
		
	bootPart.lba_start = 0;
	bootPart.lba_end = (BOOT_PART_SIZE / NX_EMMC_BLOCKSIZE) - 1;
		
	for (int i = 0; i < 2; i++){
		strcpy(bootPart.name, "BOOT");
		bootPart.name[4] = (u8)('0' + i);
		bootPart.name[5] = 0;     

		emummc_storage_set_mmc_partition(&storage, i + 1);
		utils_copystring(fsutil_getnextloc(basePath, bootPart.name), &path);

		if (!existsCheck(path))
			continue;

		SWAPCOLOR(COLOR_BLUE);
		gfx_printf("Dumping %s\n", bootPart.name);
		RESETCOLOR;
 
		emmcDumpPart(path, &storage, &bootPart);
		free(path);
	}
	emummc_storage_set_mmc_partition(&storage, 0);
	return 0;
}

int emmcDumpSpecific(char *part, char *path){
	if (!existsCheck(path))
		return 0;

	emummc_storage_set_mmc_partition(&storage, 0);
	if (connect_part(part)){
		gfx_errDisplay("dump_emmc_specific", ERR_PART_NOT_FOUND, 0);
		return 1;
	}

	SWAPCOLOR(COLOR_VIOLET);
	gfx_printf("Dumping %s\n", part);
	RESETCOLOR;
	emmcDumpPart(path, &storage, system_part);

	return 0;
}