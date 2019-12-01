#include <string.h>
#include <stdlib.h>
#include "gfx.h"
#include "fs.h"
#include "../utils/types.h"
#include "../libs/fatfs/ff.h"
#include "../utils/sprintf.h"
#include "../utils/btn.h"
#include "../gfx/gfx.h"

fs_entry fileobjects[500];
char rootpath[10] = "";
char currentpath[255] = "";

char *getnextloc(char *current, char *add){
    char *ret;
    size_t size = strlen(current) + strlen(add) + 1;
    ret = (char*) malloc (size);
    if (!strcmp(rootpath, current))
        sprintf(ret, "%s%s", current, add);
    else
        sprintf(ret, "%s/%s", current, add);

    return ret;
}

char *getprevloc(char *current){
    char *ret, *temp;
    size_t size = strlen(current) + 1;

    ret = (char*) malloc (size);
    strcpy(ret, current);

    temp = strrchr(ret, '/');
    memset(temp, '\0', 1);

    if (strlen(rootpath) > strlen(ret))
        strcpy(ret, rootpath);

    return ret;
}

fs_entry getfileobj(int spot){
    return fileobjects[spot];
}

bool checkfile(char* path){
    FRESULT fr;
    FILINFO fno;

    fr = f_stat(path, &fno);

    if (fr & FR_NO_FILE)
        return false;
    else
        return true;
}

void addobject(char* name, int spot, bool isfol, bool isarc){
    size_t size = strlen(name) + 1;
    fileobjects[spot].property = 0;

    fileobjects[spot].name = (char*) malloc (size);
    strlcpy(fileobjects[spot].name, name, size);

    if (isfol)
        fileobjects[spot].property |= (ISDIR);
    else {
        unsigned long size = 0;
        int sizes = 0;
        FILINFO fno;
        f_stat(getnextloc(currentpath, name), &fno);
        size = fno.fsize;
        
        while (size > 1024){
            size /= 1024;
            sizes++;
        }

        if (sizes > 3)
            sizes = 0;

        fileobjects[spot].property |= (1 << (4 + sizes));
        fileobjects[spot].size = size;
    }

    if (isarc)
        fileobjects[spot].property |= (ISARC);
}

int readfolder(const char *path){
    DIR dir;
    FILINFO fno;
    int folderamount = 0, res;
    
    if (res = f_opendir(&dir, path)){
        char errmes[50] = "";
        sprintf(errmes, "Error during f_opendir: %d", res);
        message(errmes, COLOR_RED);
    }

    while (!f_readdir(&dir, &fno) && fno.fname[0]){
        addobject(fno.fname, folderamount++, (fno.fattrib & AM_DIR), (fno.fattrib & AM_ARC));
    }

    f_closedir(&dir);
    return folderamount;
}

void filemenu(const char *startpath){
    int amount, res;
    strcpy(rootpath, startpath);
    strcpy(currentpath, startpath);
    amount = readfolder(currentpath);

    while (1){
        res = makefilemenu(fileobjects, amount, currentpath);
        if (res < 1){
            if (res == -2){
                if (!strcmp(rootpath, currentpath))
                    break;
                else {
                    strcpy(currentpath, getprevloc(currentpath));
                    amount = readfolder(currentpath);
                }
            }
            if (res == -1){
                break;
            }
                
            if (res == 0)
                break;
        }
        else {
            if (fileobjects[res - 1].property & ISDIR){
                strcpy(currentpath, getnextloc(currentpath, fileobjects[res - 1].name));
                amount = readfolder(currentpath);
            }
        }
    }
}