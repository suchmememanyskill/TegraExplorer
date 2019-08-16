#include <string.h>
#include "../gfx/di.h"
#include "../gfx/gfx.h"
#include "../utils/btn.h"
#include "utils.h"
#include "mainfunctions.h"
#include "../libs/fatfs/ff.h"
#include "../storage/sdmmc.h"
#include "graphics.h"

int _openfilemenu(const char *path, char *clipboardpath){
    meme_clearscreen();
    FILINFO fno;
    f_stat(path, &fno);
    char *options[4];
    int res = 0;
    int mres = -1;
    int ret = -1;

    addchartoarray("Back", options, 0);
    addchartoarray("Copy to clipboard", options, 1);
    addchartoarray("Move to clipboard", options, 2);
    addchartoarray("Delete file", options, 3);

    gfx_printf("%kPath: %s%k\n\n", COLOR_GREEN, path, COLOR_WHITE);
    int temp = (fno.fsize / 1024 / 1024);
    gfx_printf("Size MB: %d", temp);
    
    res = gfx_menulist(160, options, 4);
    switch(res){
        case 2:
            ret = 0;
            strcpy(clipboardpath, path);
            break;
        case 3:
            ret = 1;
            strcpy(clipboardpath, path);
            break;
        case 4:
            mres = messagebox("Are you sure you want to delete this file?\nPower to confirm\nVOL to cancel");
            if (mres == 0) f_unlink(path);
            break;
        default:
            break;
    }

    meme_clearscreen();
    return ret;
}

void sdexplorer(char *items[], unsigned int *muhbits){
    int value = 1;
    int copymode = -1;
    int folderamount = 0;
    char path[255] = "sd:/";
    char clipboard[255] = "";
    int temp = -1;
    //static const u32 colors[8] = {COLOR_RED, COLOR_ORANGE, COLOR_YELLOW, COLOR_GREEN, COLOR_BLUE, COLOR_VIOLET, COLOR_DEFAULT, COLOR_WHITE};
    while(1){
        gfx_clear_grey(0x1B);
        gfx_con_setpos(0, 0);
        gfx_box(0, 0, 719, 15, COLOR_WHITE);
        folderamount = readfolder(items, muhbits, path);
        gfx_printf("%k%pTegraExplorer, by SuchMemeManySkill    %d\n%k%p", COLOR_DEFAULT, COLOR_WHITE, folderamount - 2, COLOR_WHITE, COLOR_DEFAULT);
        value = fileexplorergui(items, muhbits, path, folderamount);
        
        if (value == 1) {
            if (strcmp("sd:/", path) == 0) break;
            else removepartpath(path);
        }
        else if (value == 2) {
            if (copymode != -1){
                copywithpath(clipboard, path, copymode);
                copymode = -1;
            }
        }
        else {
            if(muhbits[value - 1] & OPTION1) addpartpath(path, items[value - 1]);
            else {
                addpartpath(path, items[value - 1]);
                temp = _openfilemenu(path, clipboard);
                if (temp != -1) copymode = temp; 
                removepartpath(path);
            }
        }
    }
}