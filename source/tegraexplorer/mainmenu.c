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

MenuEntry_t mainMenuEntries[] = {
    {.R = 255, .G = 255, .B = 255, .skip = 1, .name = "-- Main Menu --"},
    {.G = 255, .name = "SD:/"},
    {.optionUnion = COLORTORGB(COLOR_YELLOW), .name = "emmc:/SYSTEM"},
    {.B = 255, .G = 255, .name = "Test Controllers"},
    {.R = 255, .name = "Cause an exception"},
    {.optionUnion = COLORTORGB(COLOR_ORANGE), .name = "View dumped keys"},
    {.R = 255, .name = "Reboot to payload"}
};

void HandleSD(){
    gfx_clearscreen();
    TConf.curExplorerLoc = LOC_SD;
    if (!sd_mount()){
        gfx_printf("Sd is not mounted!");
        hidWait();
    }
    else
        FileExplorer("sd:/");
}

void HandleEMMC(){
    if (connectMMC(MMC_CONN_EMMC))
        return;
    
    ErrCode_t err = mountMMCPart("SYSTEM");
    if (err.err){
        DrawError(err);
        return;
    }
    
    FileExplorer("bis:/");
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

menuPaths mainMenuPaths[] = {
    NULL,
    HandleSD,
    HandleEMMC,
    TestControllers,
    CrashTE,
    ViewKeys,
    RebootToPayload
};

void EnterMainMenu(){
    while (1){
        mainMenuEntries[2].hide = !TConf.keysDumped;
        mainMenuEntries[5].hide = !TConf.keysDumped;
        FunctionMenuHandler(mainMenuEntries, ARR_LEN(mainMenuEntries), mainMenuPaths, ALWAYSREDRAW);
    }
}

