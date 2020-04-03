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

extern bool sd_mount();
extern void sd_unmount();
extern int launch_payload(char *path);
extern bool sd_inited;
extern bool sd_mounted;

int res = 0, meter = 0;

void MainMenu_SDCard(){
    fileexplorer("SD:/", 0);
}

void MainMenu_EMMC(){
    gfx_clearscreen();
    gfx_printf("You're about to enter EMMC\nModifying anything here\n        can result in a BRICK!\n\nPlease only continue\n    if you know what you're doing\n\nPress Vol+/- to return\n");
    if (gfx_makewaitmenu("Press Power to enter", 4)){
        connect_mmc(SYSMMC);

        if (!mount_mmc(emmc_fs_entries[res - 2], res - 1))
            fileexplorer("emmc:/", 1);
    }
}

void MainMenu_EMUMMC(){
    connect_mmc(EMUMMC);

    if (!mount_mmc(emmc_fs_entries[res - 5], res - 4))
        fileexplorer("emmc:/", 1);   
}

void MainMenu_MountSD(){
    (sd_mounted) ? sd_unmount() : sd_mount();
}

void MainMenu_Tools(){
    //res = makemenu(toolsmenu, 8);
    res = menu_make(mainmenu_tools, 6, "-- Tools Menu --");

    switch(res){
        case TOOLS_DISPLAY_INFO:
            displayinfo();
            break;
        case TOOLS_DISPLAY_GPIO:
            displaygpio();
            break;
        case TOOLS_DUMPFIRMWARE:
            dumpfirmware(SYSMMC);
            break;
        case TOOLS_DUMPUSERSAVE:
            if ((res = utils_mmcMenu()) > 0)
                dumpusersaves(res);

            break;
        case TOOLS_DUMP_BOOT:
            dump_emmc_parts(PART_BOOT | PART_PKG2, SYSMMC);
            break;
    }
}

void MainMenu_SDFormat(){
    //res = makemenu(formatmenu, 4);
    res = menu_make(mainmenu_format, 3, "-- Format Menu --");

    if (res > 0){
        gfx_clearscreen();
        gfx_printf("Are you sure you want to format your sd?\nThis will delete everything on your SD card\nThis action is irreversible!\n\nPress Vol+/- to cancel\n");
        if(gfx_makewaitmenu("Press Power to continue", 10)){
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
    MainMenu_EMMC,
    MainMenu_EMMC,
    MainMenu_EMUMMC,
    MainMenu_EMUMMC,
    MainMenu_EMUMMC,
    MainMenu_MountSD,
    MainMenu_Tools,
    MainMenu_SDFormat,
    MainMenu_Credits,
    MainMenu_Exit,
};

void RunMenuOption(int option){
    if (option != 11)
        meter = 0;
    if (option > 0)
        mainmenu_functions[option - 1]();
}
void te_main(){
    int setter;

    if (dump_biskeys() == -1){
        gfx_errDisplay("dump_biskey", ERR_BISKEY_DUMP_FAILED, 0);
        for (int i = 1; i <= 3; i++)
            mainmenu_main[i].property |= ISHIDE;
    }

    if (emummc_load_cfg()){
        for (int i = 4; i <= 6; i++)
            mainmenu_main[i].property |= ISHIDE;
    }

    disconnect_mmc();

    while (1){
        //fillmainmenu();

        setter = sd_mounted;

        if (emu_cfg.enabled){
            for (int i = 4; i <= 6; i++)
                SETBIT(mainmenu_main[i].property, ISHIDE, !setter);
        }

        SETBIT(mainmenu_main[0].property, ISHIDE, !setter);
        mainmenu_main[7].name = (menu_sd_states[!setter]);

        setter = sd_inited;
        SETBIT(mainmenu_main[9].property, ISHIDE, !setter);

        res = menu_make(mainmenu_main, 12, "-- Main Menu --") + 1;
        RunMenuOption(res);
    }
}