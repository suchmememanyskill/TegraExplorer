#include "emmcfile.h"
#include "../gfx/gfx.h"
#include "emummc.h"
#include "mountmanager.h"
#include <libs/fatfs/ff.h>
#include "nx_emmc.h"
#include <mem/heap.h>
#include "../err.h"
#include "../tegraexplorer/tconf.h"
#include "../gfx/gfxutils.h"
#include "../hid/hid.h"
#include <storage/nx_sd.h>
#include <string.h>

// Uses default storage in nx_emmc.c
// Expects the correct mmc & partition to be set
ErrCode_t EmmcDumpToFile(const char *path, u32 lba_start, u32 lba_end){
    FIL fp;
    u32 curLba = lba_start;
    u32 totalSectors = lba_end - lba_start + 1;
    
    int res = f_open(&fp, path, FA_CREATE_ALWAYS | FA_WRITE);
    if (res){
        return newErrCode(res);
    }

    ErrCode_t err = newErrCode(0);

    u8 *buff = malloc(TConf.FSBuffSize);

    u32 x, y;
    gfx_con_getpos(&x, &y);

    while (totalSectors > 0){
        u32 num = MIN(totalSectors, TConf.FSBuffSize / NX_EMMC_BLOCKSIZE);
        if (!emummc_storage_read(&emmc_storage, curLba, num, buff)){
            err = newErrCode(TE_ERR_EMMC_READ_FAIL);
            break;
        }

        if ((res = f_write(&fp, buff, num * NX_EMMC_BLOCKSIZE, NULL))){
            err = newErrCode(res);
            break;
        }

        curLba += num;
        totalSectors -= num;

        u32 percent = ((curLba - lba_start) * 100) / ((lba_end - lba_start + 1));
        gfx_printf("[%3d%%]", percent);
        gfx_con_setpos(x, y);
    }

    f_close(&fp);
    free(buff);
    return err;
}

ErrCode_t DumpEmmcPart(const char *path, const char *part){
    const u32 BOOT_PART_SIZE = emmc_storage.ext_csd.boot_mult << 17;

    if (!sd_mount())
        return newErrCode(TE_ERR_NO_SD);

    // maybe check for existing file here

    if (!memcmp(part, "BOOT0", 5)){
        emummc_storage_set_mmc_partition(&emmc_storage, 1);
        return EmmcDumpToFile(path, 0, (BOOT_PART_SIZE / NX_EMMC_BLOCKSIZE) - 1);
    }
    else if (!memcmp(part, "BOOT1", 5)){
        emummc_storage_set_mmc_partition(&emmc_storage, 2);
        return EmmcDumpToFile(path, 0, (BOOT_PART_SIZE / NX_EMMC_BLOCKSIZE) - 1);
    }
    else {
        emummc_storage_set_mmc_partition(&emmc_storage, 0);

        emmc_part_t *system_part = nx_emmc_part_find(GetCurGPT(), part);
        if (!system_part)
            return newErrCode(TE_ERR_PARTITION_NOT_FOUND);

        return EmmcDumpToFile(path, system_part->lba_start, system_part->lba_end);
    }
}