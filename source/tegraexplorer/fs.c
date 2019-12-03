#include <string.h>
#include <stdlib.h>
#include "gfx.h"
#include "fs.h"
#include "../utils/types.h"
#include "../libs/fatfs/ff.h"
#include "../utils/sprintf.h"
#include "../utils/btn.h"
#include "../gfx/gfx.h"
#include "../utils/util.h"

fs_entry fileobjects[500];
char rootpath[10] = "";
char currentpath[300] = "";
char clipboard[300] = "";
u8 clipboardhelper = 0;
extern const char sizevalues[4][3];
extern int launch_payload(char *path);

menu_item explfilemenu[8] = {
    {"-- File Menu --", COLOR_BLUE, -1, 0},
    {"FILE", COLOR_GREEN, -1, 0},
    {"\nSIZE", COLOR_VIOLET, -1, 0},
    {"\n\n\nBack", COLOR_WHITE, -1, 1},
    {"\nCopy to clipboard", COLOR_BLUE, COPY, 1},
    {"Move to clipboard", COLOR_BLUE, MOVE, 1},
    {"\nDelete file", COLOR_RED, DELETE, 1},
    {"\nLaunch Payload", COLOR_ORANGE, PAYLOAD, 1}
};

void writecurpath(const char *in){
    /*
    if (currentpath != NULL)
        free(currentpath);

    size_t len = strlen(in) + 1;
    currentpath = (char*) malloc (len);
    strcpy(currentpath, in);
    */
   strcpy(currentpath, in);
}

void writeclipboard(const char *in, bool operation, bool folder){
    //if (clipboard != NULL)
    //    free(clipboard);

    clipboardhelper = 0;
    
    if (operation)
        clipboardhelper |= (OPERATION);

    if (folder)
        clipboardhelper |= (ISDIR);

    /*
    size_t len = strlen(in) + 1;
    clipboard = (char*) malloc (len);
    strcpy(clipboard, in);
    */
   strcpy(clipboard, in);
}

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

    if (fileobjects[spot].name != NULL){
        free(fileobjects[spot].name);
        fileobjects[spot].name = NULL;
    }

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
    int amount, res, tempint;
    char temp[100];
    strcpy(rootpath, startpath);
    writecurpath(startpath);
    amount = readfolder(currentpath);

    while (1){
        res = makefilemenu(fileobjects, amount, currentpath);
        if (res < 1){
            if (res == -2){
                if (!strcmp(rootpath, currentpath))
                    break;
                else {
                    writecurpath(getprevloc(currentpath));
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
                writecurpath(getnextloc(currentpath, fileobjects[res - 1].name));
                amount = readfolder(currentpath);
            }
            else {
                strlcpy(explfilemenu[1].name, fileobjects[res - 1].name, 43);
            
                for (tempint = 4; tempint < 8; tempint++)
                    if ((fileobjects[res - 1].property & (1 << tempint)))
                        break;

                sprintf(temp, "\nSize: %d %s", fileobjects[res - 1].size, sizevalues[tempint - 4]);
                strcpy(explfilemenu[2].name, temp);

                if (strstr(fileobjects[res - 1].name, ".bin") != NULL)
                    explfilemenu[7].property = 1;
                else
                    explfilemenu[7].property = -1;

                res = makemenu(explfilemenu, 8);

                switch (res){
                    case COPY:
                        writeclipboard(getnextloc(currentpath, fileobjects[res - 1].name), false, false);
                        break;
                    case MOVE:
                        writeclipboard(getnextloc(currentpath, fileobjects[res - 1].name), true, false);
                        break;
                    case DELETE:
                        msleep(100);
                        sprintf(temp, "Do you want to delete:\n%s\n\nPress Power to confirm\nPress Vol+/- to cancel", fileobjects[res - 1].name);
                        res = message(temp, COLOR_RED);
                        if (res & BTN_POWER){
                            f_unlink(getnextloc(currentpath, fileobjects[res - 1].name));
                            amount = readfolder(currentpath);
                        }
                        break;
                    case PAYLOAD:
                        launch_payload(getnextloc(currentpath, fileobjects[res - 1].name));
                        break;
                }
            }
        }
    }
}