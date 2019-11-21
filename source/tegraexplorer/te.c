#include <stdio.h>
#include <string.h>
#include "te.h"
#include "gfx.h"
#include "../utils/util.h"

extern bool sd_mount();
extern void sd_unmount();
bool sd_mounted = false;

menu_item mainmenu[MAINMENU_AMOUNT] = {
    {"[SD:/] SD CARD", COLOR_GREEN, 1, 0},
    {"[EMMC:/] ?\n", COLOR_GREEN, 2, 0},
    {"Mount/Unmount SD", COLOR_WHITE, 3, 0},
    {"Tools\n", COLOR_VIOLET, 4, 0},
    {"Credits", COLOR_WHITE, 5, 0},
    {"Exit", COLOR_WHITE, 6, 0} 
};

menu_item shutdownmenu[4] = {
    {"Reboot to RCM", COLOR_VIOLET, 1, 0},
    {"Reboot normally", COLOR_ORANGE, 2, 0},
    {"Power off\n", COLOR_BLUE, 3, 0},
    {"Back", COLOR_WHITE, 4, 0}
};

void fillmainmenu(){
    int i;

    for (i = 0; i < MAINMENU_AMOUNT; i++){
        switch (i + 1) {
            case 1:
                if (sd_mounted)
                    mainmenu[i].property = 1;
                else
                    mainmenu[i].property = -1;
                break;
            case 3:
                if (sd_mounted){
                    mainmenu[i].property = 1;
                    strcpy(mainmenu[i].name, "Unmount SD");
                }
                else {
                    mainmenu[i].property = 0;
                    strcpy(mainmenu[i].name, "Mount SD");
                }
                break;
        }
    }
}

void te_main(){
    int res;

    sd_mounted = sd_mount();

    while (1){
        fillmainmenu();
        res = makemenu(mainmenu, MAINMENU_AMOUNT);

        if (res == 3){
            if (sd_mounted){
                sd_mounted = false;
                sd_unmount();
            }
            else
                sd_mounted = sd_mount();
        }

        if (res == 5)
            message(CREDITS_MESSAGE, COLOR_WHITE);

        if (res == 6){
            res = makemenu(shutdownmenu, 4);
            if (res == 1)
                reboot_rcm();
            else if (res == 2)
                reboot_normal();
            else if (res == 3)
                power_off();
        }
    }
}