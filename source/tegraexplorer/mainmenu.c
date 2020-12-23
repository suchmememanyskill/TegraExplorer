#include "mainMenu.h"
#include "../gfx/gfx.h"
#include "../gfx/gfxutils.h"
#include "../gfx/menu.h"
#include "tools.h"
#include "../hid/hid.h"
#include "../fs/menus/explorer.h"
#include <utils/btn.h>
#include <storage/nx_sd.h>

MenuEntry_t mainMenuEntries[] = {
    {.R = 255, .G = 255, .B = 255, .skip = 1, .name = "-- Main Menu --"},
    {.G = 255, .name = "SD:/"},
    {.B = 255, .G = 255, .name = "Test Controllers"},
    {.R = 255, .name = "Reboot to payload"}
};

void HandleSD(){
    gfx_clearscreen();
    if (!sd_mount()){
        gfx_printf("Sd is not mounted!");
        hidWait();
    }
    else
        FileExplorer("sd:/");
}

menuPaths mainMenuPaths[] = {
    NULL,
    HandleSD,
    TestControllers,
    RebootToPayload
};

void EnterMainMenu(){
    while (1){
        FunctionMenuHandler(mainMenuEntries, 4, mainMenuPaths, false);
    }
}

