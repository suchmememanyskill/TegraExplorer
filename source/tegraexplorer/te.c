#include <stdio.h>
#include <string.h>
#include "te.h"
#include "gfx.h"
#include "../utils/util.h"

extern bool sd_mount();
extern void sd_unmount();

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

int calcmenuitems(){
    int amount = 0, i;

    for (i = 0; i < MAINMENU_AMOUNT; i++)
        if (mainmenu[i].property >= 0)
            amount++;
    
    return amount;
}

void fillmainmenu(){
    int i;

    for (i = 0; i < MAINMENU_AMOUNT; i++){
        switch (i + 1) {
            case 1:
                if (sd_mount)
                    mainmenu[i].property = 1;
                else
                    mainmenu[i].property = -1;
                break;
            case 3:
                if (sd_mount)
                    mainmenu[i].property = 1;
                else
                    mainmenu[i].property = -1;
                break;
        }
    }
}

void te_main(){
    int res;

    while (1){
        fillmainmenu();
        res = makemenu(mainmenu, calcmenuitems());

        if (res == 5)
            message(CREDITS_MESSAGE, COLOR_GREEN);

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