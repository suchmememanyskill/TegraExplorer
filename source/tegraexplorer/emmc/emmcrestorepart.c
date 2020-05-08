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
#include "../../mem/heap.h"
#include "../../storage/nx_emmc.h"
#include "../common/types.h"
#include "../utils/utils.h"
#include "../fs/fsutils.h"

extern sdmmc_storage_t storage;
extern emmc_part_t *system_part;

int emmcRestorePart(char *path, sdmmc_storage_t *mmcstorage, emmc_part_t *part, bool warnings){
    FIL fp;
    FILINFO fno;
    u8 *buf;
	u32 lba_curr = part->lba_start;
	u64 bytesWritten = 0;
    u32 totalSectorsDest = part->lba_end - part->lba_start + 1;
    u64 totalSizeDest = (u64)((u64)totalSectorsDest << 9);
    u64 totalSize;
    u32 num = 0;
    u32 pct = 0;
    int res;

    gfx_printf("Initializing\r");

    buf = calloc(BUFSIZE, sizeof(u8));

    if (!buf){
        gfx_errDisplay("restore_emmc_part", ERR_MEM_ALLOC_FAILED, 1);
        return -1;
    }

    if ((res = f_stat(path, &fno))){
        gfx_errDisplay("restore_emmc_part", res, 2);
        return -1;
    }

    totalSize = fno.fsize;
    u64 totalSectors = totalSize / NX_EMMC_BLOCKSIZE;

    if (totalSize > totalSizeDest){
        gfx_errDisplay("restore_emmc_part", ERR_FILE_TOO_BIG_FOR_DEST, 3);
        return -1;
    }

    gfx_printf("Flashing %s\n", part->name);

    if (totalSize < totalSizeDest && warnings){
        SWAPCOLOR(COLOR_ORANGE);
        gfx_printf("File is too small for destination.\nDo you want to flash it anyway?\n\nB to Cancel\n");
        u32 btnres = gfx_makewaitmenu(
            "A to Confirm",
            3
        );

        RESETCOLOR;

        if (!btnres){
            gfx_printf("\nCancelled: %s\n\n", part->name);
            return 0;
        }
        else
            gfx_printf("\nFlashing %s\n", part->name);
    }

    if ((res = f_open(&fp, path, FA_OPEN_ALWAYS | FA_READ))){
        gfx_errDisplay("restore_emmc_part", res, 4);
        return -1;
    }

    while (totalSectors > 0){
        num = MIN(totalSectors, 128);

        if ((res = f_read(&fp, buf, num * NX_EMMC_BLOCKSIZE, NULL))){
            gfx_errDisplay("restore_emmc_part", res, 5);
            return -1;
        }

        if (!emummc_storage_write(mmcstorage, lba_curr, num, buf)){
            gfx_errDisplay("restore_emmc_part", ERR_EMMC_WRITE_FAILED, 2);
            return -1;
        }

        lba_curr += num;
        totalSectors -= num;
        bytesWritten += (u64)(num * NX_EMMC_BLOCKSIZE);

        pct = (u64)(bytesWritten * (u64)100) / (u64)(fno.fsize);
        gfx_printf("Progress: %d%%\r", pct);
    }

    gfx_printf("                \r");
    f_close(&fp);
    free(buf);
    return 0;
}

// function replaced by new mmc implementation. Will be removed at some point
/*
int restore_emmc_file(char *path, const char *target, u8 partition, u8 mmctype){
    connect_mmc(mmctype);

    if (partition){
        emmc_part_t bootPart;
        const u32 BOOT_PART_SIZE = storage.ext_csd.boot_mult << 17;
        memset(&bootPart, 0, sizeof(bootPart));

        bootPart.lba_start = 0;
        bootPart.lba_end = (BOOT_PART_SIZE / NX_EMMC_BLOCKSIZE) - 1;

        strcpy(bootPart.name, target);

        emummc_storage_set_mmc_partition(&storage, partition);
        restore_emmc_part(path, &storage, &bootPart);
    }
    else {
        if (connect_part(target)){
            gfx_errDisplay("restore_emmc_part", ERR_PART_NOT_FOUND, 0);
            return -1;
        }
        restore_emmc_part(path, &storage, system_part);
    }
    return 0;
}

int restore_bis_using_file(char *path, u8 mmctype){
    f_mkdir("sd:/tegraexplorer");
    f_mkdir("sd:/tegraexplorer/bis");
    gfx_clearscreen();

    if (extract_bis_file(path, "sd:/tegraexplorer/bis"))
        return -1;

    SWAPBGCOLOR(COLOR_RED);
    SWAPCOLOR(COLOR_DEFAULT);
    gfx_printf("\nAre you sure you want to flash these files\nThis could leave your switch unbootable!\nTarget is SysMMC\n\nVol +/- to cancel\n");
    if (!gfx_makewaitmenu(
        "Power to confirm",
        10
    ))
    {
        return 0;
    }
    RESETCOLOR;

    gfx_printf("\nRestoring BIS...\n\n");

    restore_emmc_file("sd:/tegraexplorer/bis/BOOT0.bin", "BOOT0", 1, mmctype);
    restore_emmc_file("sd:/tegraexplorer/bis/BOOT1.bin", "BOOT1", 2, mmctype);
    restore_emmc_file("sd:/tegraexplorer/bis/BCPKG2-1-Normal-Main", pkg2names[0], 0, mmctype);
    restore_emmc_file("sd:/tegraexplorer/bis/BCPKG2-1-Normal-Main", pkg2names[1], 0, mmctype);
    restore_emmc_file("sd:/tegraexplorer/bis/BCPKG2-3-SafeMode-Main", pkg2names[2], 0, mmctype);
    restore_emmc_file("sd:/tegraexplorer/bis/BCPKG2-3-SafeMode-Main", pkg2names[3], 0, mmctype);

    gfx_printf("\n\nDone!\nPress any button to return");
    btn_wait();

    return 0;
}
*/

emmc_part_t *mmcFindPart(char *path, short mmcType){
    char *filename, *extention, *path_local;
    emmc_part_t *part;

    utils_copystring(path, &path_local);
    filename = strrchr(path_local, '/') + 1;
    extention = strrchr(path_local, '.');

    if (extention != NULL)
        *extention = '\0';

    if (checkGptRules(filename)){
        gfx_errDisplay("mmcFindPart", ERR_CANNOT_COPY_FILE_TO_FS_PART, 1);
        free(path_local);
        return NULL;
    }

    part = nx_emmc_part_find(selectGpt(mmcType), filename);

    if (part != NULL){
        emummc_storage_set_mmc_partition(&storage, 0);
        free(path_local);
        return part;
    }

    if (!strcmp(filename, "BOOT0") || !strcmp(filename, "BOOT1")){
        const u32 BOOT_PART_SIZE = storage.ext_csd.boot_mult << 17;
        part = calloc(1, sizeof(emmc_part_t));

        part->lba_start = 0;
        part->lba_end = (BOOT_PART_SIZE / NX_EMMC_BLOCKSIZE) - 1;

        strcpy(part->name, filename);

        emummc_storage_set_mmc_partition(&storage, (!strcmp(filename, "BOOT0")) ? 1 : 2);
        free(path_local);
        return part;
    }

    //gfx_printf("Path: %s\nFilename: %s", path, filename);
    //btn_wait();
    gfx_errDisplay("mmcFindPart", ERR_NO_DESTINATION, 2);
    free(path_local);
    return NULL;
}

int mmcFlashFile(char *path, short mmcType, bool warnings){
    emmc_part_t *part;
    int res;

    part = mmcFindPart(path, mmcType);
    if (part != NULL){
        res = emmcRestorePart(path, &storage, part, warnings);
        emummc_storage_set_mmc_partition(&storage, 0);
        return res;
    }
    return 1;
}
