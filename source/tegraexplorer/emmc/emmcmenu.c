#include <string.h>
#include "emmc.h"
#include "../../mem/heap.h"
#include "../../utils/types.h"
#include "../../libs/fatfs/ff.h"
#include "../../hid/hid.h"
#include "../../gfx/gfx.h"
#include "../../utils/util.h"
#include "../../storage/nx_emmc.h"
#include "../../mem/mc.h"
#include "../../mem/emc.h"
#include "../../storage/emummc.h"
#include "../../config/config.h"
#include "../common/common.h"
#include "../gfx/gfxutils.h"
#include "../../utils/list.h"
#include "../../mem/heap.h"
#include "emmcmenu.h"
#include "../utils/utils.h"
#include "../gfx/menu.h"
#include "../fs/fsmenu.h"
#include "emmcoperations.h"
#include "../fs/fsutils.h"
#include "../utils/menuUtils.h"

menu_entry *mmcMenuEntries = NULL;
extern sdmmc_storage_t storage;
extern emmc_part_t *system_part;
extern char *clipboard;
extern u8 clipboardhelper;
extern bool sd_mounted;


void addEntry(emmc_part_t *part, u8 property_GPT, int spot){
    u32 storage = 0;
    u8 property = 0;

    if (property_GPT & isFS){
        storage = (u32)(property_GPT & 0x7F);
        property = ISDIR;
    }
    else {
        u64 size = 0;
        u32 sizes = 0;

        size = (u64)(part->lba_end + 1 - part->lba_start);
        size *= (u64)NX_EMMC_BLOCKSIZE;

        while (size > 1024){
            size /= 1024;
            sizes++;
        }

        if (sizes > 3)
            sizes = 3;

        property |= SETSIZE(sizes);
        storage = (u32)size;
    }

    mu_copySingle(part->name, storage, property, &mmcMenuEntries[spot]);
}

int fillMmcMenu(short mmcType){
    int count = 4, i;

    if (mmcMenuEntries != NULL)
        mu_clearObjects(&mmcMenuEntries);

    link_t *gpt = selectGpt(mmcType);

    LIST_FOREACH_ENTRY(emmc_part_t, part, gpt, link)
        count++;

    mu_createObjects(count, &mmcMenuEntries);

    if (!sd_mounted)
      sd_mount();

    SETBIT(mmcmenu_filemenu[3].property, ISHIDE, !sd_mounted);
    SETBIT(mmcmenu_start[1].property, ISHIDE, !sd_mounted);

    for (i = 0; i < 4; i++)
        mu_copySingle(mmcmenu_start[i].name, mmcmenu_start[i].storage, mmcmenu_start[i].property, &mmcMenuEntries[i]);

    LIST_FOREACH_ENTRY(emmc_part_t, part, gpt, link){
        addEntry(part, checkGptRules(part->name), i++);
    }

    return count;
}

int handleEntries(short mmcType, menu_entry part){
    int res = 0;

    if (part.property & ISDIR){
        if (returnpkg1info().ver < 0){
            gfx_errDisplay("emmcMakeMenu", ERR_BISKEY_DUMP_FAILED, 0);
            return -1;
        }
        if (!mount_mmc(part.name, part.storage))
            fileexplorer("emmc:/", 1);
    }
    else {
        /*
        if (mmcmenu_filemenu[1].name != NULL)
            free(mmcmenu_filemenu[1].name);

        utils_copystring(part.name, &mmcmenu_filemenu[1].name);
        */

        mu_copySingle(part.name, mmcmenu_filemenu[1].storage, mmcmenu_filemenu[1].property, &mmcmenu_filemenu[1]);

        if ((menu_make(mmcmenu_filemenu, 4, "-- RAW PARTITION --")) < 3)
            return 0;

        f_mkdir("sd:/tegraexplorer");
        f_mkdir("sd:/tegraexplorer/partition_dumps");

        gfx_clearscreen();

        if (part.isNull){
            res = emmcDumpBoot("sd:/tegraexplorer/partition_dumps");
        }
        else {
            //gfx_printf("Dumping %s...\n", part.name);
            res = emmcDumpSpecific(part.name, fsutil_getnextloc("sd:/tegraexplorer/partition_dumps", part.name));
        }

        if (!res){
            gfx_printf("\nDone!");
            hidWait();
        }
    }

    return 0;
}

int makeMmcMenu(short mmcType){
    int count, selection;
    count = fillMmcMenu(mmcType);
    connect_mmc(mmcType);
    while (1){
        selection = menu_make(mmcMenuEntries, count, (mmcType == SYSMMC) ? "-- SYSMMC --" : "-- EMUMMC --");

        switch(selection){
            case -1:
            case 0:
                return 0;
            case 1:
                gfx_clearscreen();
                f_mkdir("sd:/tegraexplorer");
                f_mkdir("sd:/tegraexplorer/partition_dumps");

                for (int i = 0; i < count; i++){
                    if (mmcMenuEntries[i].property & (ISMENU | ISDIR))
                        continue;

                    //gfx_printf("Dumping %s...\n", mmcMenuEntries[i].name);
                    emmcDumpSpecific(mmcMenuEntries[i].name, fsutil_getnextloc("sd:/tegraexplorer/partition_dumps", mmcMenuEntries[i].name));
                }

                gfx_printf("\nDone!");
                hidWait();
                break;
            case 2:
                if (!(clipboardhelper & ISDIR) && (clipboardhelper & OPERATIONCOPY)){
                    gfx_clearscreen();
                    if (!mmcFlashFile(clipboard, mmcType, true)){
                        gfx_printf("\nDone!");
                        hidWait();
                    }
                    clipboardhelper = 0;
                }
                else
                    gfx_errDisplay("mmcMenu", ERR_EMPTY_CLIPBOARD, 0);
                break;
            default:
                handleEntries(mmcType, mmcMenuEntries[selection]);
                break;
        }
    }
}
