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

MenuEntry_t mainMenuEntries[] = {
    {.R = 255, .G = 255, .B = 255, .skip = 1, .name = "-- Main Menu --"},
    {.G = 255, .name = "SD:/"},
    {.B = 255, .G = 255, .name = "Test Controllers"},
    {.R = 255, .name = "Cause an exception"},
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

void CrashTE(){
    gfx_printf("%d", *((int*)0));
}

menuPaths mainMenuPaths[] = {
    NULL,
    HandleSD,
    TestControllers,
    CrashTE,
    RebootToPayload
};

void EnterMainMenu(){
    while (1){
        FunctionMenuHandler(mainMenuEntries, ARR_LEN(mainMenuEntries), mainMenuPaths, 0);
    }
}

