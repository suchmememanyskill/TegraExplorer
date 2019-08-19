#include <string.h>
#include <stdio.h>
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

static u8 bis_keys[4][0x20];

void meme_main(){
    utils_gfx_init();
    //dump_keys();

    /*
    sdmmc_storage_t storage;
    sdmmc_t sdmmc;

    sdmmc_storage_init_mmc(&storage, &sdmmc, SDMMC_4, SDMMC_BUS_WIDTH_8, 4);
    sdmmc_storage_set_mmc_partition(&storage, 1); */

    dump_biskeys(bis_keys);

    char *options[5];
    char *itemsinfolder[1000];
    unsigned int muhbits[1000];
    bool sd_mounted = false;
    sd_mounted = sd_mount();
    if (!sd_mounted) messagebox("\nSD INIT FAILED");

    while (1){
        int i = 0, ret = 0;
        if (sd_mounted){
            addchartoarray("[SD:/] SD card", options, i);
            i++;
        }
        addchartoarray("[emmc:/] SYSTEM", options, i);
        addchartoarray("\nTools", options, i+1);
        addchartoarray("About", options, i+2);
        addchartoarray("Exit", options, i+3);

        meme_clearscreen();
        ret = gfx_menulist(32, options, (i + 4));

        if (strcmp(options[ret - 1], "[SD:/] SD card") == 0){
            sdexplorer(itemsinfolder, muhbits, "sd:/");
        }
        else if (strcmp(options[ret - 1], "About") == 0){
            messagebox(ABOUT_MESSAGE);
        }
        else if (strcmp(options[ret - 1], "[emmc:/] SYSTEM") == 0){
            sdexplorer(itemsinfolder, muhbits, "emmc:/");
        }
        else if (strcmp(options[ret - 1], "\nTools") == 0){
            meme_clearscreen();
            addchartoarray("Back", options, 0);
            addchartoarray("\nPrint BISKEYS", options, 1);
            if (!sd_mounted) addchartoarray("Mount SD", options, 2);
            else addchartoarray("Unmount SD", options, 2);
            ret = gfx_menulist(32, options, 3);
            switch(ret){
                case 2:
                    meme_clearscreen();
                    gfx_printf("\nBisKey 0:\n");
                    gfx_hexdump(0, bis_keys[0], 0x20 * sizeof(u8));
                    gfx_printf("\n\nBisKey 1:\n");
                    gfx_hexdump(0, bis_keys[1], 0x20 * sizeof(u8));
                    gfx_printf("\n\nBisKey 2 + 3:\n");
                    gfx_hexdump(0, bis_keys[2], 0x20 * sizeof(u8));
                    btn_wait();
                    break;
                case 3:
                    if (sd_mounted){
                        sd_unmount();
                        sd_mounted = false;
                    }
                    else {
                        sd_mounted = sd_mount();
                        if (!sd_mounted) messagebox("\nSD INIT FAILED");
                    }
                    break;
            }
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