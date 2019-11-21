#include <stdio.h>
#include <string.h>
#include "te.h"
#include "gfx.h"
#include "../utils/util.h"
#include "tools.h"

extern bool sd_mount();
extern void sd_unmount();
bool sd_mounted = false;

menu_item mainmenu[MAINMENU_AMOUNT] = {
    {"[SD:/] SD CARD", COLOR_GREEN, 1, 1},
    {"[EMMC:/] ?", COLOR_GREEN, 2, 1},
    {"\nMount/Unmount SD", COLOR_WHITE, 3, 1},
    {"Tools", COLOR_VIOLET, 4, 1},
    {"\nCredits", COLOR_WHITE, 5, 1},
    {"Exit", COLOR_WHITE, 6, 1}
};

menu_item shutdownmenu[5] = {
    {"-- EXIT --\n", COLOR_ORANGE, -1, 0},
    {"Back", COLOR_WHITE, 1, 1},
    {"\nReboot to RCM", COLOR_VIOLET, 2, 1},
    {"Reboot normally", COLOR_ORANGE, 3, 1},
    {"Power off", COLOR_BLUE, 4, 1}
};

menu_item toolsmenu[3] = {
    {"-- TOOLS --\n", COLOR_VIOLET, -1, 0},
    {"Back", COLOR_WHITE, 1, 1},
    {"\nDisplay Console Info", COLOR_GREEN, 2, 1}
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

        if (res == 4){
            res = makemenu(toolsmenu, 3);

            if (res == 2)
                displayinfo();
        }

        if (res == 5)
            message(CREDITS_MESSAGE, COLOR_WHITE);

        if (res == 6){
            res = makemenu(shutdownmenu, 5);
            if (res == 2)
                reboot_rcm();
            else if (res == 3)
                reboot_normal();
            else if (res == 4)
                power_off(); //todo declock bpmp
        }
    }
}