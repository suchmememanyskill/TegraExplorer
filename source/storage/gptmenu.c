#include "gptmenu.h"
#include "../gfx/gfx.h"
#include "../gfx/menu.h"
#include "../gfx/gfxutils.h"
#include "../utils/vector.h"
#include "mountmanager.h"
#include <utils/list.h>
#include <string.h>
#include "nx_emmc.h"
#include <mem/heap.h>
#include "../fs/menus/explorer.h"
#include "../err.h"
#include "../tegraexplorer/tconf.h"
#include "emmcfile.h"
#include <storage/nx_sd.h>
#include "../fs/fsutils.h"

MenuEntry_t GptMenuHeader[] = {
    {.optionUnion = COLORTORGB(COLOR_ORANGE), .name = "<- Back"},
    {.optionUnion = COLORTORGB(COLOR_GREY) | SKIPBIT, .name = "Clipboard -> Partition\n"},
    {.optionUnion = COLORTORGB(COLOR_BLUE), .name = "BOOT0", .icon = 128, .showSize = 1, .size = 4, .sizeDef = 2}, 
    {.optionUnion = COLORTORGB(COLOR_BLUE), .name = "BOOT1", .icon = 128, .showSize = 1, .size = 4, .sizeDef = 2} 
};

const char *GptFSEntries[] = {
    "PRODINFOF",
    "SAFE",
    "SYSTEM",
    "USER"
};

void GptMenu(u8 MMCType){
    if (connectMMC(MMCType))
        return;

    Vector_t GptMenu = newVec(sizeof(MenuEntry_t), 15);
    GptMenu.count = ARR_LEN(GptMenuHeader);
    memcpy(GptMenu.data, GptMenuHeader, sizeof(MenuEntry_t) * ARR_LEN(GptMenuHeader));

    link_t *gpt = GetCurGPT();

    LIST_FOREACH_ENTRY(emmc_part_t, part, gpt, link) {
        MenuEntry_t entry = {.optionUnion = COLORTORGB(COLOR_VIOLET), .icon = 128, .name = part->name};
        u64 total = (part->lba_end - part->lba_start) / 2 + 1;
        u8 type = 1;
        while (total > 1024){
            total /= 1024;
            type++;
        }

        if (type > 3)
            type = 3;

        entry.showSize = 1;
        entry.size = total;
        entry.sizeDef = type;

        for (int i = 0; i < ARR_LEN(GptFSEntries); i++){
            if (!strcmp(part->name, GptFSEntries[i])){
                entry.optionUnion = COLORTORGB(COLOR_WHITE);
                entry.icon = 127;
                break;
            }
        }

        vecAddElem(&GptMenu, entry);
    }

    vecDefArray(MenuEntry_t*, entries, GptMenu);
    int res = 0;
    while (1){
        gfx_clearscreen();
        gfx_printf((MMCType == MMC_CONN_EMMC) ? "-- Emmc --\n\n" : "-- Emummc --\n\n");

        res = newMenu(&GptMenu, res, 40, 20, ALWAYSREDRAW | ENABLEB, GptMenu.count);

        if (res < 2){
            break;
        }
        else if (entries[res].icon == 127){
            unmountMMCPart();
            ErrCode_t err = mountMMCPart(entries[res].name);
            if (err.err){
                DrawError(err);
            }
            else {
                if (TConf.curExplorerLoc > LOC_SD)
                    ResetCopyParams();
                    
                TConf.curExplorerLoc = LOC_EMMC;
                FileExplorer("bis:/");
            }
        }
        else {
            if (!sd_mount())
                continue;

            gfx_clearscreen();
            gfx_printf("Do you want to dump %s?  ", entries[res].name);
            if (MakeYesNoHorzMenu(3, COLOR_DEFAULT)){
                gfx_putc('\n');
                RESETCOLOR;
                gfx_printf("Dumping %s... ", entries[res].name);

                f_mkdir("sd:/tegraexplorer");
                f_mkdir("sd:/tegraexplorer/Dumps");

                char *path = CombinePaths("sd:/tegraexplorer/Dumps", entries[res].name);

                ErrCode_t a = DumpEmmcPart(path, entries[res].name);
                if (a.err)
                    DrawError(a);
            }
        }
    }

    free(GptMenu.data);
}