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

MenuEntry_t GptMenuHeader[] = {
    {.optionUnion = COLORTORGB(COLOR_ORANGE), .name = "<- Back"},
    {.optionUnion = COLORTORGB(COLOR_GREY) | SKIPBIT, .name = "Clipboard -> Partition"},
    {.optionUnion = COLORTORGB(COLOR_GREY) | SKIPBIT, .name = "\nBoot0/1"} // Should be blue when implemented
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
    GptMenu.count = 3;
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

        if (res < 3){
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
            DrawError(newErrCode(TE_ERR_UNIMPLEMENTED));
        }
    }

    free(GptMenu.data);
}