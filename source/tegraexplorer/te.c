#include <stdio.h>
#include <string.h>
#include "te.h"
#include "gfx.h"
#include "../utils/util.h"
#include "tools.h"
#include "fs.h"
#include "../utils/btn.h"
#include "emmc.h"

extern bool sd_mount();
extern void sd_unmount();
extern bool return_sd_mounted(int value);
extern int launch_payload(char *path);

menu_item mainmenu[MAINMENU_AMOUNT] = {
    {"[SD:/] SD CARD\n", COLOR_GREEN, SD_CARD, 1},
    {"[SYSTEM:/] EMMC", COLOR_ORANGE, EMMC_SYS, 1},
    {"[USER:/] EMMC", COLOR_ORANGE, EMMC_USR, 1},
    {"[SAFE:/] EMMC", COLOR_ORANGE, EMMC_SAF, 1},
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

menu_item toolsmenu[5] = {
    {"-- TOOLS --\n", COLOR_VIOLET, -1, 0},
    {"Back", COLOR_WHITE, -1, 1},
    {"\nDisplay Console Info", COLOR_GREEN, DISPLAY_INFO, 1},
    {"Display GPIO pins", COLOR_VIOLET, DISPLAY_GPIO, 1},
    {"Dump Firmware", COLOR_BLUE, DUMPFIRMWARE, 1}
};

menu_item formatmenu[4] = {
    {"-- FORMAT SD --\n", COLOR_RED, -1, 0},
    {"Back\n", COLOR_WHITE, -1, 1},
    {"Format entire SD to FAT32", COLOR_RED, FORMAT_ALL_FAT32, 1},
    {"Format for EmuMMC setup (FAT32/RAW)", COLOR_RED, FORMAT_EMUMMC, 1}
};

const char emmc_entries[3][8] = {
    "SAFE",
    "SYSTEM",
    "USER"
};

void fillmainmenu(){
    int i;

    for (i = 0; i < MAINMENU_AMOUNT; i++){
        switch (i + 1) {
            case 1:
            case 7:
                if (return_sd_mounted(i + 1))
                    mainmenu[i].property = 1;
                else
                    mainmenu[i].property = -1;
                break;
            case 5:
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
    int res;

    if (dump_biskeys() == -1){
        message("Biskeys failed to dump!\nEmmc will not be mounted!", COLOR_RED);
        mainmenu[1].property = -1;
        mainmenu[2].property = -1;
        mainmenu[3].property = -1;
    }

    while (1){
        fillmainmenu();
        res = makemenu(mainmenu, MAINMENU_AMOUNT);

        switch(res){
            case SD_CARD:
                fileexplorer("SD:/");
                break;
            
            case EMMC_SAF:
            case EMMC_SYS:
            case EMMC_USR:

            if (makewaitmenu("You're about to enter EMMC\nModifying anything here\n        can result in a BRICK!\n\nPlease only continue\n    if you know what you're doing\n\nPress Vol+/- to return\n", "Press Power to enter", 4)){
                if (!mount_emmc(emmc_entries[res - 2], res - 1)){
                    fileexplorer("emmc:/");
                }
                else
                    message("EMMC failed to mount!", COLOR_RED);
            }

            break;

            case MOUNT_SD:
                if (return_sd_mounted(1))
                    sd_unmount();
                else
                    sd_mount();

                break;

            case TOOLS:
                res = makemenu(toolsmenu, 5);

                switch(res){
                    case DISPLAY_INFO:
                        displayinfo();
                        break;
                    case DISPLAY_GPIO:
                        displaygpio();
                        break;
                    case DUMPFIRMWARE:
                        dumpfirmware();
                        break;
                }
                break;
            
            case SD_FORMAT:
                res = makemenu(formatmenu, 4);

                if (res >= 0){
                    if(makewaitmenu("Are you sure you want to format your sd?\nThis will delete everything on your SD card\nThis action is irreversible!\n\nPress Vol+/- to cancel\n", "Press Power to continue", 10)){
                        if (format(res)){
                            sd_unmount();
                        }
                    }
                }

                break;

            case CREDITS:
                message(CREDITS_MESSAGE, COLOR_WHITE);
                break;

            case EXIT:
                if (return_sd_mounted(1)){  
                    if (checkfile("/bootloader/update.bin"))
                        shutdownmenu[5].property = 1;
                    else
                        shutdownmenu[5].property = -1;

                    if (checkfile("/atmosphere/reboot_payload.bin"))
                        shutdownmenu[6].property = 1;
                    else
                        shutdownmenu[6].property = -1;
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

                break;
        }
    }
}