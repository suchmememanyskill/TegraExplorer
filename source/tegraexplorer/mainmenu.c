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

MenuEntry_t mainMenuEntries[] = {
    {.R = 255, .G = 255, .B = 255, .skip = 1, .name = "-- Main Menu --"},
    {.G = 255, .name = "SD:/"},
    {.optionUnion = COLORTORGB(COLOR_YELLOW), .name = "Emmc"},
    {.optionUnion = COLORTORGB(COLOR_YELLOW), .name = "Emummc"},
    {.B = 255, .G = 255, .name = "Test Controllers"},
    {.R = 255, .name = "Cause an exception"},
    {.R = 255, .name = "Partition the sd"},
    {.optionUnion = COLORTORGB(COLOR_BLUE), .name = "Dump Firmware"},
    {.optionUnion = COLORTORGB(COLOR_ORANGE), .name = "View dumped keys"},
    {.optionUnion = COLORTORGB(COLOR_ORANGE)},
    {.R = 255, .name = "Reboot to payload"},
    {.R = 255, .name = "Reboot to RCM"}
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

void CrashTE(){
    gfx_printf("%d", *((int*)0));
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

    gfx_printf("\n\nPkg1 ID: '%s', kb %d", TConf.pkg1ID, TConf.pkg1ver);

    hidWait();
}

extern bool sd_mounted;
extern bool is_sd_inited;

void MountOrUnmountSD(){
    (sd_mounted) ? sd_unmount() : sd_mount();
}

menuPaths mainMenuPaths[] = {
    NULL,
    HandleSD,
    HandleEMMC,
    HandleEMUMMC,
    TestControllers,
    CrashTE,
    FormatSD,
    DumpSysFw,
    ViewKeys,
    MountOrUnmountSD,
    RebootToPayload,
    reboot_rcm
};

void EnterMainMenu(){
    while (1){
        if (sd_get_card_removed())
            sd_unmount();

        mainMenuEntries[1].hide = !sd_mounted;
        mainMenuEntries[2].hide = !TConf.keysDumped;
        mainMenuEntries[3].hide = (!TConf.keysDumped || !emu_cfg.enabled || !sd_mounted);
        mainMenuEntries[6].hide = (!is_sd_inited || sd_get_card_removed());
        mainMenuEntries[7].hide = !TConf.keysDumped;
        mainMenuEntries[9].name = (sd_mounted) ? "Unmount SD" : "Mount SD";
        FunctionMenuHandler(mainMenuEntries, ARR_LEN(mainMenuEntries), mainMenuPaths, ALWAYSREDRAW);
    }
}

