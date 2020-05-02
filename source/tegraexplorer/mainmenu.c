#include <stdio.h>
#include <string.h>
#include "mainmenu.h"
#include "../utils/util.h"
#include "utils/tools.h"
#include "../utils/btn.h"
#include "emmc/emmc.h"
#include "../storage/emummc.h"
#include "script/functions.h"

#include "common/common.h"
#include "gfx/menu.h"

#include "utils/utils.h"
#include "gfx/gfxutils.h"
#include "fs/fsutils.h"
#include "fs/fsmenu.h"
#include "emmc/emmcoperations.h"
#include "emmc/emmcmenu.h"
#include "../storage/nx_sd.h"
//#include "../hid/joycon.h"
#include "../hid/hid.h"
/*
extern bool sd_mount();
extern void sd_unmount();
*/
extern int launch_payload(char *path);
extern bool sd_inited;
extern bool sd_mounted;
extern bool disableB;

int res = 0, meter = 0;

void MainMenu_SDCard(){
    fileexplorer("SD:/", 0);
}

void MainMenu_EMMC(){
    if (gfx_defaultWaitMenu("You're about to enter EMMC\nModifying anything here can result in a BRICK!\n\nPlease only continue if you know what you're doing", 4)){
        /*
        connect_mmc(SYSMMC);

        if (!mount_mmc(emmc_fs_entries[res - 2], res - 1))
            fileexplorer("emmc:/", 1);
        */
       makeMmcMenu(SYSMMC);
    }
}

void MainMenu_EMUMMC(){
    /*
    connect_mmc(EMUMMC);

    if (!mount_mmc(emmc_fs_entries[res - 5], res - 4))
        fileexplorer("emmc:/", 1);
    */
   makeMmcMenu(EMUMMC);
}

void MainMenu_MountSD(){
    (sd_mounted) ? sd_unmount() : sd_mount();
}

void MainMenu_Tools(){
    //res = makemenu(toolsmenu, 8);
    res = menu_make(mainmenu_tools, 5, "-- Tools Menu --");

    switch(res){
        case TOOLS_DISPLAY_INFO:
            displayinfo();
            break;
        case TOOLS_DISPLAY_GPIO:
            displaygpio();
            //makeMmcMenu(SYSMMC);
            break;
        case TOOLS_DUMPFIRMWARE:
            dumpfirmware(SYSMMC);
            break;
        case TOOLS_DUMPUSERSAVE:
            if ((res = utils_mmcMenu()) > 0)
                dumpusersaves(res);

            break;
    }
}

void MainMenu_SDFormat(){
    //res = makemenu(formatmenu, 4);
    res = menu_make(mainmenu_format, 3, "-- Format Menu --");

    if (res > 0){
        if(gfx_defaultWaitMenu("Are you sure you want to format your sd?\nThis will delete everything on your SD card!\nThis action is irreversible!", 10)){
            if (format(res)){
                sd_unmount();
            }
        }
    }
}

void MainMenu_Credits(){
    if (++meter >= 3)
        gfx_errDisplay("credits", 53, 0);
    gfx_message(COLOR_WHITE, mainmenu_credits);
}

void MainMenu_Exit(){
    if (sd_mounted){
        SETBIT(mainmenu_shutdown[4].property, ISHIDE, !fsutil_checkfile("/bootloader/update.bin"));
        SETBIT(mainmenu_shutdown[5].property, ISHIDE, !fsutil_checkfile("/atmosphere/reboot_payload.bin"));
    }
    else {
        for (int i = 4; i <= 5; i++)
            SETBIT(mainmenu_shutdown[i].property, ISHIDE, 1);
    }

    res = menu_make(mainmenu_shutdown, 6, "-- Shutdown Menu --");

    switch(res){
        case SHUTDOWN_REBOOT_RCM:
            reboot_rcm();
                    
        case SHUTDOWN_REBOOT_NORMAL:
            reboot_normal();

        case SHUTDOWN_POWER_OFF:
            power_off();

        case SHUTDOWN_HEKATE:
            launch_payload("/bootloader/update.bin");
                    
        case SHUTDOWN_AMS:
            launch_payload("/atmosphere/reboot_payload.bin");
    } //todo declock bpmp
}

func_void_ptr mainmenu_functions[] = {
    MainMenu_SDCard,
    MainMenu_EMMC,
    MainMenu_EMUMMC,
    MainMenu_MountSD,
    MainMenu_Tools,
    MainMenu_SDFormat,
    MainMenu_Credits,
    MainMenu_Exit,
};

void RunMenuOption(int option){
    if (option != 7)
        meter = 0;
    if (option > 0)
        mainmenu_functions[option - 1]();
}
void te_main(){
    int setter;

    if (dump_biskeys() == -1){
        gfx_errDisplay("dump_biskey", ERR_BISKEY_DUMP_FAILED, 0);
        //mainmenu_main[1].property |= ISHIDE;
    }

    //gfx_message(COLOR_ORANGE, "%d %d %d", sd_mount(), sd_mounted, sd_inited);
    sd_mount();

    if (emummc_load_cfg()){
        mainmenu_main[2].property |= ISHIDE;
    }

    dumpGpt();

    disconnect_mmc();

    hidInit();

    while (1){
        //fillmainmenu();

        setter = sd_mounted;

        if (emu_cfg.enabled){
            SETBIT(mainmenu_main[2].property, ISHIDE, !setter);
        }

        SETBIT(mainmenu_main[0].property, ISHIDE, !setter);
        mainmenu_main[3].name = (menu_sd_states[!setter]);

        setter = sd_inited;
        SETBIT(mainmenu_main[5].property, ISHIDE, !setter);

        disableB = true;
        res = menu_make(mainmenu_main, 8, "-- Main Menu --") + 1;
        disableB = false;

        RunMenuOption(res);
    }
}