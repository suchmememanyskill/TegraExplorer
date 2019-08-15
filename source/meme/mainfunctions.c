#include <string.h>
#include "../gfx/di.h"
#include "../gfx/gfx.h"
#include "../utils/btn.h"
#include "utils.h"
#include "mainfunctions.h"
#include "../libs/fatfs/ff.h"
#include "../storage/sdmmc.h"
#include "graphics.h"

void sdexplorer(char *items[], unsigned int *muhbits){
    int value = 1;
    int folderamount = 0;
    char path[255] = "sd:/";
    static const u32 colors[7] = {COLOR_RED, COLOR_ORANGE, COLOR_YELLOW, COLOR_GREEN, COLOR_BLUE, COLOR_VIOLET, COLOR_DEFAULT};
    while(1){
        gfx_clear_grey(0x1B);
        gfx_con_setpos(0, 0);
        gfx_box(0, 0, 719, 15, COLOR_GREEN);
        folderamount = readfolder(items, muhbits, path);
        gfx_printf("%k%pTegraExplorer, made by SuchMeme        %d\n%k%p", colors[6], colors[3], folderamount - 2, colors[3], colors[6]);
        value = fileexplorergui(items, muhbits, path, folderamount);
        
        if (value == 1) {}
        else if (value == 2) {
            if (strcmp("sd:/", path) == 0) break;
            else removepartpath(path);
        }
        else {
            if(muhbits[value - 1] & OPTION1) addpartpath(path, items[value - 1]);
        }
    }
}