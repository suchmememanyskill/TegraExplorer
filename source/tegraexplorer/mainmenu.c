#include "mainmenu.h"
#include "../gfx/gfx.h"
#include "../gfx/gfxutils.h"
#include "../gfx/menu.h"
#include "tools.h"
#include "../hid/hid.h"
#include "../fs/menus/explorer.h"
#include <utils/btn.h>
#include <storage/nx_sd.h>
#include "tconf.h"
#include "../keys/keys.h"
#include "../storage/mountmanager.h"
#include "../storage/gptmenu.h"
#include "../storage/emummc.h"
#include <utils/util.h>
#include "../fs/fsutils.h"
#include <soc/fuse.h>

enum {
    MainExplore = 0,
    MainBrowseSd,
    MainMountSd,
    MainBrowseEmmc,
    MainBrowseEmummc,
    MainTools,
    MainPartitionSd,
    MainDumpFw,
    MainViewKeys,
    MainViewCredits,
    MainExit,
    MainPowerOff,
    MainRebootRCM,
    MainRebootHekate,
    MainRebootAMS
};

MenuEntry_t mainMenuEntries[] = {
    [MainExplore] = {.optionUnion = COLORTORGB(COLOR_WHITE) | SKIPBIT, .name = "-- Explore --"},
    [MainBrowseSd] = {.optionUnion = COLORTORGB(COLOR_GREEN), .name = "Browse SD"},
    [MainMountSd] = {.optionUnion = COLORTORGB(COLOR_YELLOW)}, // To mount/unmount the SD
    [MainBrowseEmmc] = {.optionUnion = COLORTORGB(COLOR_BLUE), .name = "Browse EMMC"},
    [MainBrowseEmummc] = {.optionUnion = COLORTORGB(COLOR_BLUE), .name = "Browse EMUMMC"},
    [MainTools] = {.optionUnion = COLORTORGB(COLOR_WHITE) | SKIPBIT, .name = "\n-- Tools --"},
    [MainPartitionSd] = {.optionUnion = COLORTORGB(COLOR_ORANGE), .name = "Partition the sd"},
    [MainDumpFw] = {.optionUnion = COLORTORGB(COLOR_BLUE), .name = "Dump Firmware"},
    [MainViewKeys] = {.optionUnion = COLORTORGB(COLOR_YELLOW), .name = "View dumped keys"},
    [MainViewCredits] = {.optionUnion = COLORTORGB(COLOR_YELLOW), .name = "Credits"},
    [MainExit] = {.optionUnion = COLORTORGB(COLOR_WHITE) | SKIPBIT, .name = "\n-- Exit --"},
    [MainPowerOff] = {.optionUnion = COLORTORGB(COLOR_VIOLET), .name = "Power off"},
    [MainRebootRCM] = {.optionUnion = COLORTORGB(COLOR_VIOLET), .name = "Reboot to RCM"},
    [MainRebootHekate] = {.optionUnion = COLORTORGB(COLOR_VIOLET), .name = "Reboot to bootloader/update.bin"},
    [MainRebootAMS] = {.optionUnion = COLORTORGB(COLOR_VIOLET), .name = "Reboot to atmosphere/reboot_payload.bin"}
};

void HandleSD(){
    gfx_clearscreen();
    TConf.curExplorerLoc = LOC_SD;
    if (!sd_mount() || sd_get_card_removed()){
        gfx_printf("Sd is not mounted!");
        hidWait();
    }
    else
        FileExplorer("sd:/");
}

void HandleEMMC(){
   GptMenu(MMC_CONN_EMMC);
}

void HandleEMUMMC(){
    GptMenu(MMC_CONN_EMUMMC);
}

void ViewKeys(){
    gfx_clearscreen();
    for (int i = 0; i < 3; i++){
        gfx_printf("\nBis key 0%d:   ", i);
        PrintKey(dumpedKeys.bis_key[i], AES_128_KEY_SIZE * 2);
    }
    
    gfx_printf("\nMaster key 0: ");
    PrintKey(dumpedKeys.master_key, AES_128_KEY_SIZE);
    gfx_printf("\nHeader key:   ");
    PrintKey(dumpedKeys.header_key, AES_128_KEY_SIZE * 2);
    gfx_printf("\nSave mac key: ");
    PrintKey(dumpedKeys.save_mac_key, AES_128_KEY_SIZE);

    u8 fuseCount = 0;
    for (u32 i = 0; i < 32; i++){
        if ((fuse_read_odm(7) >> i) & 1)
            fuseCount++;
    }

    gfx_printf("\n\nPkg1 ID: '%s' (kb %d)\nFuse count: %d", TConf.pkg1ID, TConf.pkg1ver, fuseCount);

    hidWait();
}

void ViewCredits(){
    gfx_clearscreen();
    gfx_printf("\nTegraexplorer v%d.%d.%d\nBy SuchMemeManySkill\n\nBased on Lockpick_RCM & Hekate, from shchmue & CTCaer", LP_VER_MJ, LP_VER_MN, LP_VER_BF);
    hidWait();
}

extern bool sd_mounted;
extern bool is_sd_inited;
extern int launch_payload(char *path);

void RebootToAMS(){
    launch_payload("sd:/atmosphere/reboot_payload.bin");
}

void RebootToHekate(){
    launch_payload("sd:/bootloader/update.bin");
}

void MountOrUnmountSD(){
    (sd_mounted) ? sd_unmount() : sd_mount();
}

menuPaths mainMenuPaths[] = {
    [MainBrowseSd] = HandleSD,
    [MainMountSd] = MountOrUnmountSD,
    [MainBrowseEmmc] = HandleEMMC,
    [MainBrowseEmummc] = HandleEMUMMC,
    [MainPartitionSd] = FormatSD,
    [MainDumpFw] = DumpSysFw,
    [MainViewKeys] = ViewKeys,
    [MainRebootAMS] = RebootToAMS,
    [MainRebootHekate] = RebootToHekate,
    [MainRebootRCM] = reboot_rcm,
    [MainPowerOff] = power_off,
    [MainViewCredits] = ViewCredits,
};

void EnterMainMenu(){
    while (1){
        if (sd_get_card_removed())
            sd_unmount();

        // -- Explore --
        mainMenuEntries[MainBrowseSd].hide = !sd_mounted;
        mainMenuEntries[MainMountSd].name = (sd_mounted) ? "Unmount SD" : "Mount SD";
        mainMenuEntries[MainBrowseEmummc].hide = (!emu_cfg.enabled || !sd_mounted);

        // -- Tools --
        mainMenuEntries[MainPartitionSd].hide = (!is_sd_inited || sd_get_card_removed());
        mainMenuEntries[MainDumpFw].hide = (!TConf.keysDumped || !sd_mounted);
        mainMenuEntries[MainViewKeys].hide = !TConf.keysDumped;

        // -- Exit --
        mainMenuEntries[MainRebootAMS].hide = (!sd_mounted || !FileExists("sd:/atmosphere/reboot_payload.bin"));
        mainMenuEntries[MainRebootHekate].hide = (!sd_mounted || !FileExists("sd:/bootloader/update.bin"));

        FunctionMenuHandler(mainMenuEntries, ARR_LEN(mainMenuEntries), mainMenuPaths, ALWAYSREDRAW);
    }
}

