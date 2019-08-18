#include <string.h>
#include "../gfx/di.h"
#include "../gfx/gfx.h"
#include "../utils/btn.h"
#include "utils.h"
#include "main.h"
#include "mainfunctions.h"
#include "../libs/fatfs/ff.h"
#include "../storage/sdmmc.h"
#include "../utils/util.h"
#include "../sec/se.h"
#include "graphics.h"
#include "external_utils.h"

extern bool sd_mount();
extern void sd_unmount();

void meme_main(){
    utils_gfx_init();   
    
    sdmmc_storage_t storage;
    sdmmc_t sdmmc;

    sdmmc_storage_init_mmc(&storage, &sdmmc, SDMMC_4, SDMMC_BUS_WIDTH_8, 4);
    sdmmc_storage_set_mmc_partition(&storage, 1);
    
    char *options[5];
    char *itemsinfolder[1000];
    unsigned int muhbits[1000];
    bool sd_mounted = false;
    sd_mounted = sd_mount();

    while (1){
        int i = 0, ret = 0;
        if (sd_mounted){
            addchartoarray("[SD:/] SD card", options, i);
            i++;
        }
        else messagebox("\nSD INIT FAILED");
        addchartoarray("\nAbout", options, i);
        addchartoarray("Exit", options, i+1);

        meme_clearscreen();
        ret = gfx_menulist(32, options, (i + 2));

        if (strcmp(options[ret - 1], "[SD:/] SD card") == 0){
            sdexplorer(itemsinfolder, muhbits);
        }
        else if (strcmp(options[ret - 1], "\nAbout") == 0){
            messagebox(ABOUT_MESSAGE);
        }
        else {
            meme_clearscreen();
            addchartoarray("Back", options, 0);
            addchartoarray("\nReboot to RCM", options, 1);
            addchartoarray("Reboot normally", options, 2);
            addchartoarray("Power off", options, 3);
            ret = gfx_menulist(32, options, 4);
            if (ret != 1) sd_unmount();
            switch(ret){
                case 2:
                    reboot_rcm();
                case 3:
                    reboot_normal();
                case 4:
                    power_off();
                default:
                    break;
            }
        }
    }

    //if (sd_mounted){
    

    //write file and folder menu
    //make clipboard and shit like that
    //figure out time from keys.c
    //figure out how to reboot to payloads https://github.com/CTCaer/hekate/blob/101c8bc1d0813da10016be771a9919c9e8112277/bootloader/main.c#L266
    //gfx_printf("%k\n\nExited main loop, vol+ to reboot to rcm\nvol- to reboot normally\npower to power off\n", COLOR_GREEN);
    //}
    //else gfx_printf("%k%pSD INIT FAILED\n\nvol+ to reboot to rcm\nvol- to reboot normally\npower to power off", COLOR_RED, COLOR_DEFAULT);

    //utils_waitforpower();
}