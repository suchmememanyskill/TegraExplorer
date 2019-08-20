#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "../gfx/di.h"
#include "../gfx/gfx.h"
#include "../utils/btn.h"
#include "../utils/util.h"
#include "utils.h"
#include "../libs/fatfs/ff.h"
#include "../storage/sdmmc.h"
#include "graphics.h"
#include "external_utils.h"

void utils_gfx_init(){
    display_backlight_brightness(100, 1000);
    gfx_clear_grey(0x1B);
    gfx_con_setpos(0, 0);
}

void removepartpath(char *path, char *root){
    char *ret;
    char temproot[20];
    ret = strrchr(path, '/');
    memset(ret, '\0', 1);
    sprintf(temproot, "%s%s", root, "/");
    if (strcmp(path, root) == 0) strcpy(path, temproot);
}

int dumptosd(const char *path){
    f_mkdir("sd:/tegraexplorer");
    f_mkdir("sd:/tegraexplorer/nanddump");
    FILINFO fno;
    int res = 0;
    f_stat(path, &fno);
    char pathname[PATHSIZE];
    char foldername[75];
    strcpy(pathname, path);
    removepartpath(pathname, "emmc:");
    strcpy(foldername, strrchr(pathname, '/'));
    if (strcmp(foldername, "/") == 0) strcpy(foldername, "/root");
    sprintf(pathname, "%s%s", "sd:/tegraexplorer/nanddump", foldername);
    f_mkdir(pathname);
    sprintf(pathname, "%s%s%s%s", "sd:/tegraexplorer/nanddump", foldername, "/", fno.fname);
    res = copy(path, pathname, 1);
    return res;
}

void addpartpath(char *path, char *add, char *root){
    if (strcmp(path, root) != 0) strcat(path, "/");
    strcat(path, add);
}

void return_readable_byte_amounts(unsigned long int size, char *in){
    char type[3];
    unsigned long int sizetemp = size;
    int muhbytes = 0;
    while(sizetemp > 1024){
        muhbytes++;
        sizetemp = sizetemp / 1024;
    }

    switch(muhbytes){
        case 0:
            strcpy(type, "B");
            break;
        case 1:
            strcpy(type, "KB");
            break;
        case 2:
            strcpy(type, "MB");
            break;
        case 3:
            strcpy(type, "GB");
            break;
        default:
            strcpy(type, "GB");
            break;
    }
    sprintf(in, "%d%s", sizetemp, type);
}

int getfilesize(const char *path){
    FILINFO fno;
    f_stat(path, &fno);
    return fno.fsize;
}

void addchartoarray(char *add, char *items[], int spot){
    size_t size = strlen(add) + 1;
    items[spot] = (char*) malloc (size);
    strlcpy(items[spot], add, size);
}

void mallocandaddfolderbit(unsigned int *muhbits, int spot, bool value){
    muhbits[spot] = (unsigned int) malloc (sizeof(int));
    if (value) muhbits[spot] |= (OPTION1);
    //ff.h line 368
}

int readfolder(char *items[], unsigned int *muhbits, const char *path){
    DIR dir;
    FILINFO fno;
    int i = 2;
    addchartoarray("Current folder -> One folder up", items, 0);
    addchartoarray("Clipboard -> Current folder", items, 1);
    mallocandaddfolderbit(muhbits, 0, true);
    mallocandaddfolderbit(muhbits, 1, true);

    
    if (f_opendir(&dir, path)) {
        gfx_printf("\nFailed to open %s", path);
        return -1;
    }
    else {
        while (!f_readdir(&dir, &fno) && fno.fname[0]){
            addchartoarray(fno.fname, items, i);
            mallocandaddfolderbit(muhbits, i, fno.fattrib & AM_DIR);
            i++;
        }
    }
    f_closedir(&dir);
    return i;
}

int copy(const char *src, const char *dst, int print){
    FIL in;
    FIL out;
    unsigned int res = 0;
    char temp[100];
    if (strcmp(src, dst) == 0){
        //in and out are the same, aborting!
        return 2;
    }
    res = f_open(&in, src, FA_READ | FA_OPEN_EXISTING);
    if (res != FR_OK){
        //something has gone wrong
        //sprintf(temp, "%s %d", src, res);
        //messagebox(temp);

        return 1;
    }
    if (f_open(&out, dst, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK){
        //something has gone wrong
        return 1;
    }

    int BUFFSIZ = 32768;
    u64 size = f_size(&in);
    unsigned long totalsize = size, kbwritten = 0;
    void *buff = malloc(BUFFSIZ);
    int mbwritten = 0, percentage = 0;
    bool abort = false;
    if (print == 1) {
        meme_clearscreen();
        gfx_printf("press VOL- to abort the file transfer!\n\n");
    }
    while(size > BUFFSIZ){
        int res1, res2;
        res1 = f_read(&in, buff, BUFFSIZ, NULL);
        res2 = f_write(&out, buff, BUFFSIZ, NULL);

        kbwritten = kbwritten + (BUFFSIZ / 1024);
        mbwritten = kbwritten / 1024;
        percentage = (mbwritten * 100) / ((totalsize / 1024) / 1024);

        size = size - BUFFSIZ;
        if (print == 1){
            gfx_printf("Written %dMB [%k%d%k%%]\r", mbwritten, COLOR_GREEN, percentage, COLOR_WHITE);
            if (btn_read() & BTN_VOL_DOWN) size = 0, abort = true;
        }
    }
    
    if(size != 0){
        f_read(&in, buff, size, NULL);
        f_write(&out, buff, size, NULL);
    }

    f_close(&in);
    f_close(&out);

    if(abort){
        f_unlink(dst);
    }

    free(buff);

    return 0;
}

int copywithpath(const char *src, const char *dstpath, int mode, char *app){
    FILINFO fno;
    f_stat(src, &fno);
    char dst[PATHSIZE];
    strcpy(dst, dstpath);
    if (strcmp(dstpath, app) != 0) strcat(dst, "/");
    strcat(dst, fno.fname);
    int ret = -1;
    if (mode == 0) ret = copy(src, dst, 1);
    if (mode == 1) f_rename(src, dst);
    return ret;
}