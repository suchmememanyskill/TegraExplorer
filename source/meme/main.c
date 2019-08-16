#include <string.h>
#include "../gfx/di.h"
#include "../gfx/gfx.h"
#include "../utils/btn.h"
#include "utils.h"
#include "main.h"
#include "mainfunctions.h"
#include "../libs/fatfs/ff.h"
#include "../storage/sdmmc.h"
#include "graphics.h"

void meme_main(bool sdinit){
    utils_gfx_init();
    //static const u32 colors[7] = {COLOR_RED, COLOR_ORANGE, COLOR_YELLOW, COLOR_GREEN, COLOR_BLUE, COLOR_VIOLET, COLOR_DEFAULT};
    //gfx_printf("%k%pTegraExplorer, made by SuchMemeManySkill    \n%k%p", colors[6], colors[3], colors[3], colors[6]);
    /*
    sdmmc_storage_t storage;
    sdmmc_t sdmmc;

    sdmmc_storage_init_mmc(&storage, &sdmmc, SDMMC_4, SDMMC_BUS_WIDTH_8, 4);
    sdmmc_storage_set_mmc_partition(&storage, 1);
    */
    //f_rename("sd:/yeet.txt", "sd:/yote.txt");

    
    char *itemsinfolder[500];
    unsigned int muhbits[500];

    if (sdinit){
    sdexplorer(itemsinfolder, muhbits);

    //write file and folder menu
    //make clipboard and shit like that
    //figure out time from keys.c
    //figure out how to reboot to payloads https://github.com/CTCaer/hekate/blob/101c8bc1d0813da10016be771a9919c9e8112277/bootloader/main.c#L266
    gfx_printf("%k\n\nExited main loop, vol+ to reboot to rcm\nvol- to reboot normally\npower to power off\n", COLOR_GREEN);
    }
    else gfx_printf("%k%pSD INIT FAILED\n\nvol+ to reboot to rcm\nvol- to reboot normally\npower to power off", COLOR_RED, COLOR_DEFAULT);

    utils_waitforpower();
}