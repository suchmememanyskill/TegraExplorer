#include <string.h>
#include "../gfx/di.h"
#include "../gfx/gfx.h"
#include "../utils/btn.h"
#include "utils.h"
#include "main.h"
#include "../libs/fatfs/ff.h"
#include "../storage/sdmmc.h"

#define OPTION1 (1 << 0)
#define OPTION2 (1 << 1)
#define OPTION3 (1 << 2)
#define OPTION4 (1 << 3)

void meme_main(){
    utils_gfx_init();
    static const u32 colors[7] = {COLOR_RED, COLOR_ORANGE, COLOR_YELLOW, COLOR_GREEN, COLOR_BLUE, COLOR_VIOLET, COLOR_DEFAULT};
    gfx_printf("%k%pHello World!\n%k%pHi denn i think i did it\n%p%kAnother test\n", colors[1], colors[0], colors[2], colors[5], colors[6], colors[3]);

    sdmmc_storage_t storage;
    sdmmc_t sdmmc;

    sdmmc_storage_init_mmc(&storage, &sdmmc, SDMMC_4, SDMMC_BUS_WIDTH_8, 4);
    sdmmc_storage_set_mmc_partition(&storage, 1);

    //f_rename("sd:/yeet.txt", "sd:/yote.txt");

    char *itemsinfolder[250];
    unsigned int muhbits[250];
    int folderamount = 0;
    char path[100] = "sd:/";

    folderamount = readfolder(itemsinfolder, muhbits, path);

    int i = 0;
    gfx_printf("%d", folderamount);
    while(i < folderamount){
        gfx_printf("\n%s", itemsinfolder[i]);
        if (muhbits[i] & OPTION1) gfx_printf(" <DIR>");
        else gfx_printf(" <FILE>");
        i++;
    }

    utils_waitforpower();
}