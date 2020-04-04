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

menu_entry *mmcMenuEntries = NULL;

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
    int count = 2, i;

    if (mmcMenuEntries != NULL)
        clearfileobjects(&mmcMenuEntries);

    link_t *gpt = selectGpt(mmcType);

    LIST_FOREACH_ENTRY(emmc_part_t, part, gpt, link)
        count++;

    createfileobjects(count, &mmcMenuEntries);

    for (i = 0; i < 2; i++){
        utils_copystring(mmcmenu_start[i].name, &mmcMenuEntries[i].name);
        mmcMenuEntries[i].property = mmcmenu_start[i].property;
        mmcMenuEntries[i].storage = mmcmenu_start[i].storage;
    }

    LIST_FOREACH_ENTRY(emmc_part_t, part, gpt, link){
        addEntry(part, checkGptRules(part->name), i++);
    }

    return count;
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
                break; //stubbed
            default:
                if (mmcMenuEntries[selection].property & ISDIR){
                    if (!mount_mmc(mmcMenuEntries[selection].name, mmcMenuEntries[selection].storage))
                        fileexplorer("emmc:/", 1);  
                } 
        }
    }
}