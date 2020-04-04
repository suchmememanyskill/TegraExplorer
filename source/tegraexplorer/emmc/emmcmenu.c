#include <string.h>
#include "emmc.h"
#include "../../mem/heap.h"
#include "../../utils/types.h"
#include "../../libs/fatfs/ff.h"
#include "../../utils/sprintf.h"
#include "../../utils/btn.h"
#include "../../gfx/gfx.h"
#include "../../utils/util.h"
#include "../../hos/pkg1.h"
#include "../../storage/sdmmc.h"
#include "../../storage/nx_emmc.h"
#include "../../sec/tsec.h"
#include "../../soc/t210.h"
#include "../../soc/fuse.h"
#include "../../mem/mc.h"
#include "../../sec/se.h"
#include "../../soc/hw_init.h"
#include "../../mem/emc.h"
#include "../../mem/sdram.h"
#include "../../storage/emummc.h"
#include "../../config/config.h"
#include "../common/common.h"
#include "../gfx/gfxutils.h"
#include "../../utils/list.h"
#include "../../mem/heap.h"
#include "emmcmenu.h"
#include "../fs/fsreader.h"
#include "../utils/utils.h"
#include "../gfx/menu.h"
#include "../fs/fsmenu.h"
#include "emmcoperations.h"
#include "../fs/fsutils.h"

menu_entry *mmcMenuEntries = NULL;
extern sdmmc_storage_t storage;
extern emmc_part_t *system_part;
extern char *clipboard;
extern u8 clipboardhelper;

int checkGptRules(char *in){
    for (int i = 0; gpt_fs_rules[i].name != NULL; i++){
        if (!strcmp(in, gpt_fs_rules[i].name))
            return gpt_fs_rules[i].property;
    }
    return 0;
}

void addEntry(emmc_part_t *part, u8 property, int spot){
    if (mmcMenuEntries[spot].name != NULL){
        free(mmcMenuEntries[spot].name);
    }

    utils_copystring(part->name, &mmcMenuEntries[spot].name);

    if (property & isFS){
        mmcMenuEntries[spot].storage = (u32)(property & 0x7F);
        mmcMenuEntries[spot].property = ISDIR;
    }
    else {
        u64 size = 0;
        int sizes = 0;
        mmcMenuEntries[spot].property = 0;
        size = (part->lba_end + 1 - part->lba_start) * NX_EMMC_BLOCKSIZE;

        while (size > 1024){
            size /= 1024;
            sizes++;
        }

        if (sizes > 3)
            sizes = 0;

        mmcMenuEntries[spot].property |= (1 << (4 + sizes));
        mmcMenuEntries[spot].storage = (u32)size;
    }
}

int fillMmcMenu(short mmcType){
    int count = 3, i;

    if (mmcMenuEntries != NULL)
        clearfileobjects(&mmcMenuEntries);

    link_t *gpt = selectGpt(mmcType);

    LIST_FOREACH_ENTRY(emmc_part_t, part, gpt, link)
        count++;

    createfileobjects(count, &mmcMenuEntries);

    for (i = 0; i < 3; i++){
        utils_copystring(mmcmenu_start[i].name, &mmcMenuEntries[i].name);
        mmcMenuEntries[i].property = mmcmenu_start[i].property;
        mmcMenuEntries[i].storage = mmcmenu_start[i].storage;
    }

    LIST_FOREACH_ENTRY(emmc_part_t, part, gpt, link){
        addEntry(part, checkGptRules(part->name), i++);
    }

    return count;
}

int handleEntries(short mmcType, menu_entry part){
    if (part.property & ISDIR){
        if (!mount_mmc(part.name, part.storage))
            fileexplorer("emmc:/", 1);  
    }
    else {
        if (mmcmenu_filemenu[1].name != NULL)
            free(mmcmenu_filemenu[1].name);
                    
        utils_copystring(part.name, &mmcmenu_filemenu[1].name);

        if ((menu_make(mmcmenu_filemenu, 4, "-- RAW PARTITION --")) < 3)
            return 0;

        if (part.property & isBOOT){
            dump_emmc_parts(PART_BOOT, (u8)mmcType);
        }
        else {
            f_mkdir("sd:/tegraexplorer");
            f_mkdir("sd:/tegraexplorer/partition_dumps");

            gfx_clearscreen();
            gfx_printf("Dumping %s...\n", part.name);

            if (!dump_emmc_specific(part.name, fsutil_getnextloc("sd:/tegraexplorer/partition_dumps", part.name))){
                gfx_printf("\nDone!");
                btn_wait();
            }
        }
    }

    return 0;
}

emmc_part_t *mmcFindPart(char *path, short mmcType){
    char *filename, *extention;
    emmc_part_t *part;
    filename = strrchr(path, '/') + 1;
    extention = strrchr(path, '.');

    if (extention != NULL)
        *extention = '\0';

    if (checkGptRules(filename)){
        gfx_errDisplay("mmcFindPart", ERR_CANNOT_COPY_FILE_TO_FS_PART, 1);
        return NULL;
    }

    part = nx_emmc_part_find(selectGpt(mmcType), filename);

    if (part != NULL){
        emummc_storage_set_mmc_partition(&storage, 0);
        return part;
    }

    if (!strcmp(filename, "BOOT0") || !strcmp(filename, "BOOT1")){
        const u32 BOOT_PART_SIZE = storage.ext_csd.boot_mult << 17;
        part = calloc(1, sizeof(emmc_part_t));

        part->lba_start = 0;
        part->lba_end = (BOOT_PART_SIZE / NX_EMMC_BLOCKSIZE) - 1;

        strcpy(part->name, filename);

        emummc_storage_set_mmc_partition(&storage, (!strcmp(filename, "BOOT0")) ? 1 : 2);
        return part;
    }

    //gfx_printf("Path: %s\nFilename: %s", path, filename);
    //btn_wait();
    gfx_errDisplay("mmcFindPart", ERR_NO_DESTENATION, 2);
    return NULL;
}

int mmcFlashFile(char *path, short mmcType){
    emmc_part_t *part;
    part = mmcFindPart(path, mmcType);
    if (part != NULL){
        return restore_emmc_part(path, &storage, part);
    }
    clipboardhelper = 0;
    return 1;
}

int makeMmcMenu(short mmcType){
    int count, selection;
    count = fillMmcMenu(mmcType);
    connect_mmc(mmcType);
    while (1){
        selection = menu_make(mmcMenuEntries, count, (mmcType == SYSMMC) ? "-- SYSMMC --" : "-- EMUMMC --");

        switch(selection){
            case 0:
                return 0;
            case 1:
                if (!(clipboardhelper & ISDIR) && (clipboardhelper & OPERATIONCOPY)){
                    gfx_clearscreen();
                    if (!mmcFlashFile(clipboard, mmcType)){
                        gfx_printf("\nDone!");
                        btn_wait();
                    }   
                }
                else
                    gfx_errDisplay("mmcMenu", ERR_EMPTY_CLIPBOARD, 0);
                break; //stubbed
            default:
                handleEntries(mmcType, mmcMenuEntries[selection]);
                break;
        }
    }
}