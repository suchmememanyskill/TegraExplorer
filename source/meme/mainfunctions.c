#include <string.h>
#include <stdio.h>
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
    char *options[6];
    int res = 0;
    int mres = -1;
    int ret = -1;
    int i = 4;

    addchartoarray("Back\n", options, 0);
    addchartoarray("Copy to clipboard", options, 1);
    addchartoarray("Move to clipboard", options, 2);
    addchartoarray("Delete file\n", options, 3);
    if (strstr(path, ".bin") != NULL){
        addchartoarray("Launch payload", options, i);
        i++;
    }    
    if (strcmp(strstr(path, "emmc:/"), path) == 0){
        addchartoarray("Dump to SD", options, i);
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
            if (strcmp(options[res - 1], "Launch payload") == 0) launch_payload(path, 0);
            else if (strcmp(options[res - 1], "Dump to SD") == 0) {
                int res = 0;
                res = dumptosd(path);
                if (res == 1) messagebox("Copy Failed\nInput or Output is invalid");
            }
    }

    meme_clearscreen();
    return ret;
}

int dumpfirmware(char *items[], unsigned int *muhbits){
    DIR dir;
    FILINFO fno;
    char path[28] = "emmc:/Contents/registered";
    char sdpath[28] = "sd:/tegraexplorer/firmware";
    char tempnand[100] = "";
    char tempsd[100] = "";
    int ret = 0, i = 0, foldersize = 0;

    meme_clearscreen();
    gfx_printf("\nStarting copy of firmware\n\n");

    f_mkdir("sd:/tegraexplorer");
    f_mkdir("sd:/tegraexplorer/firmware");

    readfolder(items, muhbits, path);

    if (f_opendir(&dir, path)) {
        messagebox("Failed to open directory!");
        return -1;
    }

    while (!f_readdir(&dir, &fno) && fno.fname[0]){
        addchartoarray(fno.fname, items, foldersize);
        mallocandaddfolderbit(muhbits, foldersize, fno.fattrib & AM_DIR);
        foldersize++;
    }

    f_closedir(&dir);

    for (i = 0; i <= foldersize; i++){
        if (muhbits[i] & AM_DIR){
            sprintf(tempnand, "%s/%s", path, items[i]);
            if(f_opendir(&dir, tempnand)){
                messagebox("Failure opening folder");
                return -2;
            }
            sprintf(tempnand, "%s/%s/00", path, items[i]);
            sprintf(tempsd, "%s/%s", sdpath, items[i]);
            //messagebox(tempnand);
            //dumptosd(tempnand);
            //btn_wait();
            //messagebox(tempsd);
            ret = copy(tempnand, tempsd, 0);
            if (ret != 0) {
                messagebox("Copy failed! (infolder)");
                return 1;
            }
            f_closedir(&dir);
        }
        else {
            sprintf(tempnand, "%s/%s", path, items[i]);
            sprintf(tempsd, "%s/%s", sdpath, items[i]);
            ret = copy(tempnand, tempsd, 0);
            if (ret != 0) {
                messagebox("Copy failed! (infile)");
                return 1;
            }
        }
        gfx_printf("Copied %d / %d nca files\r", i + 1, foldersize + 1);
    }
    return 0;
}

void wtf(char *items[], unsigned int *muhbits){
    dumpfirmware(items, muhbits); // DOESNT WORK
}

void sdexplorer(char *items[], unsigned int *muhbits, char *rootpath){
    //dumpfirmware(items, muhbits); // WORKS ?!?!?!?!
    int value = 1;
    int copymode = -1;
    int folderamount = 0;
    char path[PATHSIZE] = "";
    static char clipboard[PATHSIZE + 1] = "";
    strcpy(path, rootpath);
    char app[20], rpp[20];
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
        if (folderamount == -1){
            messagebox("\nInvalid path\n\nReturning to main menu");
            break;
        }
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
                copywithpath(clipboard, path, copymode, app);
                copymode = -1;
            }
            else messagebox("\nThe Clipboard is empty!");
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