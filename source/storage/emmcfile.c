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
#include "../fs/fsutils.h"

// Uses default storage in nx_emmc.c
// Expects the correct mmc & partition to be set
ErrCode_t EmmcDumpToFile(const char *path, u32 lba_start, u32 lba_end, u8 force){
    FIL fp;
    u32 curLba = lba_start;
    u32 totalSectors = lba_end - lba_start + 1;
    
    if (FileExists(path) && !force){
        return newErrCode(TE_WARN_FILE_EXISTS);
    }

    int res = f_open(&fp, path, FA_CREATE_ALWAYS | FA_WRITE);
    if (res){
        return newErrCode(res);
    }

    ErrCode_t err = newErrCode(0);

    u8 *buff = malloc(TConf.FSBuffSize);

    if (!buff)
        return newErrCode(TE_ERR_MEM_ALLOC_FAIL);

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

ErrCode_t EmmcRestoreFromFile(const char *path, u32 lba_start, u32 lba_end, u8 force){
    FIL fp;
    u32 curLba = lba_start;
    u32 totalSectorsDest = lba_end - lba_start + 1;
    
    int res = f_open(&fp, path, FA_OPEN_ALWAYS | FA_READ);
    if (res)
        return newErrCode(res);

    u64 totalSizeSrc = f_size(&fp);
    u32 totalSectorsSrc = totalSizeSrc / NX_EMMC_BLOCKSIZE;

    if (totalSectorsSrc > totalSectorsDest) // We don't close the file here, oh well
        return newErrCode(TE_ERR_FILE_TOO_BIG_FOR_DEST);

    if (totalSectorsSrc < totalSectorsDest && !force)
        return newErrCode(TE_WARN_FILE_TOO_SMALL_FOR_DEST);

    u8 *buff = malloc(TConf.FSBuffSize);
    ErrCode_t err = newErrCode(0);

    if (!buff)
        return newErrCode(TE_ERR_MEM_ALLOC_FAIL);

    u32 x, y;
    gfx_con_getpos(&x, &y);

    while (totalSectorsSrc > 0){
        u32 num = MIN(totalSectorsSrc, TConf.FSBuffSize / NX_EMMC_BLOCKSIZE);

        if ((res = f_read(&fp, buff, num * NX_EMMC_BLOCKSIZE, NULL))){
            err = newErrCode(res);
            break;
        }

        if (!emummc_storage_write(&emmc_storage, curLba, num, buff)){
            err = newErrCode(TE_ERR_EMMC_WRITE_FAIL);
            break;
        }

        curLba += num;
        totalSectorsSrc -= num;

        u32 percent = ((curLba - lba_start) * 100) / ((totalSizeSrc / NX_EMMC_BLOCKSIZE));
        gfx_printf("[%3d%%]", percent);
        gfx_con_setpos(x, y);
    }

    f_close(&fp);
    free(buff);
    return err;
}

ErrCode_t DumpOrWriteEmmcPart(const char *path, const char *part, u8 write, u8 force){
    const u32 BOOT_PART_SIZE = emmc_storage.ext_csd.boot_mult << 17;
    u32 lba_start = 0;
    u32 lba_end = 0;

    if (!sd_mount())
        return newErrCode(TE_ERR_NO_SD);

    if (TConf.currentMMCConnected == MMC_CONN_None)
        return newErrCode(TE_ERR_PARTITION_NOT_FOUND);

    if (!memcmp(part, "BOOT0", 5)){
        emummc_storage_set_mmc_partition(&emmc_storage, 1);
        lba_end = (BOOT_PART_SIZE / NX_EMMC_BLOCKSIZE) - 1;
    }
    else if (!memcmp(part, "BOOT1", 5)){
        emummc_storage_set_mmc_partition(&emmc_storage, 2);
        lba_end = (BOOT_PART_SIZE / NX_EMMC_BLOCKSIZE) - 1;
    }
    else {
        emummc_storage_set_mmc_partition(&emmc_storage, 0);

        emmc_part_t *system_part = nx_emmc_part_find(GetCurGPT(), part);
        if (!system_part)
            return newErrCode(TE_ERR_PARTITION_NOT_FOUND);

        lba_start = system_part->lba_start;
        lba_end = system_part->lba_end;
    }

    return ((write) ? EmmcRestoreFromFile(path, lba_start, lba_end, force) : EmmcDumpToFile(path, lba_start, lba_end, force));
}