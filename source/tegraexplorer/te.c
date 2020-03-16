#include <stdio.h>
#include <string.h>
#include "te.h"
#include "gfx.h"
#include "../utils/util.h"
#include "tools.h"
#include "fs.h"
#include "io.h"
#include "../utils/btn.h"
#include "emmc.h"
#include "../storage/emummc.h"
#include "script.h"

extern bool sd_mount();
extern void sd_unmount();
extern bool return_sd_mounted(int value);
extern int launch_payload(char *path);

menu_item mainmenu[MAINMENU_AMOUNT] = {
    {"[SD:/] SD CARD\n", COLOR_GREEN, SD_CARD, 1},
    {"[SYSTEM:/] EMMC", COLOR_ORANGE, EMMC_SYS, 1},
    {"[USER:/] EMMC", COLOR_ORANGE, EMMC_USR, 1},
    {"[SAFE:/] EMMC", COLOR_ORANGE, EMMC_SAF, 1},
    {"\n[SYSTEM:/] EMUMMC", COLOR_BLUE, EMUMMC_SYS, 1},
    {"[USER:/] EMUMMC", COLOR_BLUE, EMUMMC_USR, 1},
    {"[SAFE:/] EMUMMC", COLOR_BLUE, EMUMMC_SAF, 1},
    {"\nMount/Unmount SD", COLOR_WHITE, MOUNT_SD, 1},
    {"Tools", COLOR_VIOLET, TOOLS, 1},
    {"SD format", COLOR_VIOLET, SD_FORMAT, 1},
    {"\nCredits", COLOR_WHITE, CREDITS, 1},
    {"Exit", COLOR_WHITE, EXIT, 1}
};

menu_item shutdownmenu[7] = {
    {"-- EXIT --\n", COLOR_ORANGE, -1, 0},
    {"Back", COLOR_WHITE, -1, 1},
    {"\nReboot to RCM", COLOR_VIOLET, REBOOT_RCM, 1},
    {"Reboot normally", COLOR_ORANGE, REBOOT_NORMAL, 1},
    {"Power off\n", COLOR_BLUE, POWER_OFF, 1},
    {"Reboot to Hekate", COLOR_GREEN, HEKATE, -1},
    {"Reboot to Atmosphere", COLOR_GREEN, AMS, -1}
};

menu_item toolsmenu[8] = {
    {"-- TOOLS --\n", COLOR_VIOLET, -1, 0},
    {"Back", COLOR_WHITE, -1, 1},
    {"\nDisplay Console Info", COLOR_GREEN, DISPLAY_INFO, 1},
    {"Display GPIO pins", COLOR_VIOLET, DISPLAY_GPIO, 1},
    {"Dump Firmware", COLOR_BLUE, DUMPFIRMWARE, 1},
    {"Dump User Saves", COLOR_YELLOW, DUMPUSERSAVE, 1},
    {"[DEBUG] Dump bis", COLOR_RED, DUMP_BOOT, 1},
    {"[DEBUG] Restore bis", COLOR_RED, RESTORE_BOOT, 1}
};

menu_item formatmenu[4] = {
    {"-- FORMAT SD --\n", COLOR_RED, -1, 0},
    {"Back\n", COLOR_WHITE, -1, 1},
    {"Format entire SD to FAT32", COLOR_RED, FORMAT_ALL_FAT32, 1},
    {"Format for EmuMMC setup (FAT32/RAW)", COLOR_RED, FORMAT_EMUMMC, 1}
};

menu_item mmcChoice[3] = {
    {"Back\n", COLOR_WHITE, -1, 1},
    {"SysMMC", COLOR_ORANGE, SYSMMC, 1},
    {"EmuMMC", COLOR_BLUE, EMUMMC, 1}
};

const char emmc_entries[3][8] = {
    "SAFE",
    "SYSTEM",
    "USER"
};

int res = 0;

void MainMenu_SDCard(){
    fileexplorer("SD:/");
}

void MainMenu_EMMC(){
    if (makewaitmenu("You're about to enter EMMC\nModifying anything here\n        can result in a BRICK!\n\nPlease only continue\n    if you know what you're doing\n\nPress Vol+/- to return\n", "Press Power to enter", 4)){
        connect_mmc(SYSMMC);

        if (!mount_mmc(emmc_entries[res - 2], res - 1))
            fileexplorer("emmc:/");
        else
            message(COLOR_RED, "EMMC failed to mount!");
    }
}

void MainMenu_EMUMMC(){
    connect_mmc(EMUMMC);

    if (!mount_mmc(emmc_entries[res - 5], res - 4))
        fileexplorer("emmc:/");
    else
        message(COLOR_RED, "EMUMMC failed to mount!");
}

void MainMenu_MountSD(){
    if (return_sd_mounted(1))
        sd_unmount();
    else
        sd_mount();
}

void MainMenu_Tools(){
    res = makemenu(toolsmenu, 8);

    switch(res){
        case DISPLAY_INFO:
            displayinfo();
            break;
        case DISPLAY_GPIO:
            displaygpio();
            break;
        case DUMPFIRMWARE:
            dumpfirmware(SYSMMC);
            break;
        case DUMPUSERSAVE:
            if (mainmenu[4].property >= 0){
                if ((res = makemenu(mmcChoice, 3)) >= 0)
                    dumpusersaves(res);
            }
            else
                dumpusersaves(SYSMMC);

            break;
        case DUMP_BOOT:
            dump_emmc_parts(PART_BOOT | PART_PKG2, SYSMMC);
            break;
        case RESTORE_BOOT:
            if (makewaitmenu(
                "WARNING!\nThis will mess with your switch boot files\nMake a nand backup beforehand!\n\nThis will pull from path:\nsd:/tegraexplorer/boot.bis\n\nVol +/- to cancel\n",
                "Power to confirm",
                5
            ))
            {
                if (emu_cfg.enabled){
                    if ((res = makemenu(mmcChoice, 3)) >= 0)
                        restore_bis_using_file("sd:/tegraexplorer/boot.bis", res);
                }
                else
                    restore_bis_using_file("sd:/tegraexplorer/boot.bis", SYSMMC);
            }
            break;
    }
}

void MainMenu_SDFormat(){
    res = makemenu(formatmenu, 4);

    if (res >= 0){
        if(makewaitmenu("Are you sure you want to format your sd?\nThis will delete everything on your SD card\nThis action is irreversible!\n\nPress Vol+/- to cancel\n", "Press Power to continue", 10)){
            if (format(res)){
                sd_unmount();
            }
        }
    }
}

void MainMenu_Credits(){
    message(COLOR_WHITE, CREDITS_MESSAGE);
}

void MainMenu_Exit(){
    if (return_sd_mounted(1)){ 
        shutdownmenu[5].property = (checkfile("/bootloader/update.bin")) ? 1 : -1;
        shutdownmenu[6].property = (checkfile("/atmosphere/reboot_payload.bin")) ? 1 : -1;
    }
    else {
        shutdownmenu[5].property = -1;
        shutdownmenu[6].property = -1;
    }

    res = makemenu(shutdownmenu, 7);

    switch(res){
        case REBOOT_RCM:
            reboot_rcm();
                    
        case REBOOT_NORMAL:
            reboot_normal();

        case POWER_OFF:
            power_off();

        case HEKATE:
            launch_payload("/bootloader/update.bin");
                    
        case AMS:
            launch_payload("/atmosphere/reboot_payload.bin");
    } //todo declock bpmp
}

part_handler mainmenu_functions[] = {
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
    if (option > 0)
        mainmenu_functions[option - 1]();
}

void fillmainmenu(){
    int i;

    for (i = 0; i < MAINMENU_AMOUNT; i++){
        switch (i + 1) {
            case 5:
            case 6:
            case 7:
                if (mainmenu[i].property == -2)
                    continue;
            case 1:
            case 10:
                if (return_sd_mounted(i + 1))
                    mainmenu[i].property = 1;
                else
                    mainmenu[i].property = -1;
                break;
            case 8:
                if (return_sd_mounted(1)){
                    mainmenu[i].property = 2;
                    strcpy(mainmenu[i].name, "\nUnmount SD");
                }
                else {
                    mainmenu[i].property = 1;
                    strcpy(mainmenu[i].name, "\nMount SD");
                }
                break;
        }
    }
}

void te_main(){

    if (dump_biskeys() == -1){
        message(COLOR_RED, "Biskeys failed to dump!\nEmmc will not be mounted!");
        mainmenu[1].property = -1;
        mainmenu[2].property = -1;
        mainmenu[3].property = -1;
    }

    if (emummc_load_cfg()){
        mainmenu[4].property = -2;
        mainmenu[5].property = -2;
        mainmenu[6].property = -2;
    }

    disconnect_mmc();

    while (1){
        fillmainmenu();
        res = makemenu(mainmenu, MAINMENU_AMOUNT);
        RunMenuOption(res);
    }
}