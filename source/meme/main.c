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

void meme_main(){
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

    sdexplorer(itemsinfolder, muhbits);

    gfx_printf("%k\n\nExited main loop, vol+ to reboot to rcm\nvol- to reboot normally\npower to power off", COLOR_GREEN);

    utils_waitforpower();
}