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
#include "../utils/utils.h"

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

    GptMenuHeader[1].optionUnion = (TConf.explorerCopyMode == CMODE_Copy) ? (COLORTORGB(COLOR_ORANGE)) : (COLORTORGB(COLOR_GREY) | SKIPBIT);

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

        if (res < 1){
            break;
        }
        else if (res == 1){
            gfx_clearscreen();
            char *fileName = CpyStr(strrchr(TConf.srcCopy, '/') + 1);

            for (int i = 0; i < strlen(fileName); i++){
                if (fileName[i] >= 'a' && fileName[i] <= 'z')
                    fileName[i] &= ~BIT(5);
            }

            gfx_printf("Are you sure you want to flash %s?  ", fileName);
            if (MakeYesNoHorzMenu(3, COLOR_DEFAULT)){
                RESETCOLOR;
                gfx_printf("\nFlashing %s... ", fileName);
                ErrCode_t a = DumpOrWriteEmmcPart(TConf.srcCopy, fileName, 1, 0);
                if (a.err){
                    if (a.err == TE_WARN_FILE_TOO_SMALL_FOR_DEST){
                        gfx_printf("\r%s is too small for the destination. Flash anyway?  ", fileName);
                        if (MakeYesNoHorzMenu(3, COLOR_DEFAULT)){
                            RESETCOLOR;
                            gfx_printf("\nFlashing %s... ", fileName);
                            a = DumpOrWriteEmmcPart(TConf.srcCopy, fileName, 1, 1);
                        }
                        else {
                            a.err = 0;
                        }
                    }
                    DrawError(a);
                }
            }
            free(fileName);
        }
        else if (entries[res].icon == 127){
            unmountMMCPart();
            ErrCode_t err = mountMMCPart(entries[res].name);
            if (err.err){
                DrawError(err);
            }
            else {
                if (TConf.keysDumped){
                    if (TConf.curExplorerLoc > LOC_SD)
                        ResetCopyParams();
                    
                    TConf.curExplorerLoc = LOC_EMMC;
                    FileExplorer("bis:/");
                }
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

                ErrCode_t a = DumpOrWriteEmmcPart(path, entries[res].name, 0, 0);
                if (a.err){
                    if (a.err == TE_WARN_FILE_EXISTS){
                        gfx_printf("\rDest file for %s exists already. Overwrite?  ", entries[res].name);
                        if (MakeYesNoHorzMenu(3, COLOR_DEFAULT)){
                            RESETCOLOR;
                            gfx_printf("\nDumping %s... ", entries[res].name);
                            a = DumpOrWriteEmmcPart(path, entries[res].name, 0, 1);
                        }
                        else {
                            a.err = 0;
                        }
                    }
                    DrawError(a);
                }
            }
        }
    }

    free(GptMenu.data);
}