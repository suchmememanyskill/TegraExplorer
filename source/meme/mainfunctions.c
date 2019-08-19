#include <string.h>
#include "../gfx/di.h"
#include "../gfx/gfx.h"
#include "../utils/btn.h"
#include "utils.h"
#include "mainfunctions.h"
#include "../libs/fatfs/ff.h"
#include "../storage/sdmmc.h"
#include "graphics.h"
#include "external_utils.h"

int _openfilemenu(char *path, char *clipboardpath){
    meme_clearscreen();
    FILINFO fno;
    f_stat(path, &fno);
    char *options[5];
    int res = 0;
    int mres = -1;
    int ret = -1;
    int i = 4;
    bool payload = false;

    addchartoarray("Back", options, 0);
    addchartoarray("Copy to clipboard", options, 1);
    addchartoarray("Move to clipboard", options, 2);
    addchartoarray("Delete file", options, 3);
    if (strstr(path, ".bin") != NULL){
        addchartoarray("Launch payload", options, i);
        payload = true;
        i++;
    }

    gfx_printf("%kPath: %s%k\n\n", COLOR_GREEN, path, COLOR_WHITE);

    char size[6];
    return_readable_byte_amounts(fno.fsize, size);

    gfx_printf("Size: %s", size);
    
    res = gfx_menulist(160, options, i);
    switch(res){
        case 1:
            break;
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
            if (payload) launch_payload(path, 0);
    }

    meme_clearscreen();
    return ret;
}

void sdexplorer(char *items[], unsigned int *muhbits, char *rootpath){
    int value = 1;
    int copymode = -1;
    int folderamount = 0;
    char path[PATHSIZE] = "sd:/";
    strcpy(path, rootpath);
    char app[20], rpp[20];
    char clipboard[PATHSIZE] = "";
    int temp = -1;
    strcpy(app, rootpath);
    strcpy(rpp, app);
    removepartpath(rpp, "rpp");
    //static const u32 colors[8] = {COLOR_RED, COLOR_ORANGE, COLOR_YELLOW, COLOR_GREEN, COLOR_BLUE, COLOR_VIOLET, COLOR_DEFAULT, COLOR_WHITE};

    while(1){
        gfx_clear_grey(0x1B);
        gfx_con_setpos(0, 0);
        gfx_box(0, 0, 719, 15, COLOR_WHITE);
        folderamount = readfolder(items, muhbits, path);
        gfx_printf("%k%pTegraExplorer - %s", COLOR_DEFAULT, COLOR_WHITE, app);
        gfx_con_setpos(39 * 16, 0);
        gfx_printf("%d\n%k%p", folderamount - 2, COLOR_WHITE, COLOR_DEFAULT);
        value = fileexplorergui(items, muhbits, path, folderamount);
        
        if (value == 1) {
            if (strcmp(app, path) == 0) break;
            else removepartpath(path, rpp);
        }
        else if (value == 2) {
            if (copymode != -1){
                copywithpath(clipboard, path, copymode);
                copymode = -1;
            }
        }
        else {
            if(muhbits[value - 1] & OPTION1) addpartpath(path, items[value - 1], app);
            else {
                addpartpath(path, items[value - 1], app);
                temp = _openfilemenu(path, clipboard);
                if (temp != -1) copymode = temp; 
                removepartpath(path, rpp);
            }
        }
    }
}