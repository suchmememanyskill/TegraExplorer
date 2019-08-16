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

void utils_gfx_init(){
    display_backlight_brightness(100, 1000);
    gfx_clear_grey(0x1B);
    gfx_con_setpos(0, 0);
}

void removepartpath(char *path){
    char *ret;
    ret = strrchr(path, '/');
    memset(ret, '\0', 1);
    if (strcmp(path, "sd:") == 0) strcpy(path, "sd:/");
}

void addpartpath(char *path, char *add){
    if (strcmp(path, "sd:/") != 0) strcat(path, "/");
    strcat(path, add);
}

void return_readable_byte_amounts(int size, char *in){
    char type[3];
    int sizetemp = size;
    int muhbytes = 0;
    strcpy(type, "B");
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

void utils_waitforpower(){
    u32 btn = btn_wait();
    if (btn & BTN_VOL_UP)
        reboot_rcm();
    else if (btn & BTN_VOL_DOWN)
        reboot_normal();
    else
        power_off();
}

void addchartoarray(char *add, char *items[], int spot){
    size_t size = strlen(add) + 1;
    items[spot] = (char*) malloc (size);
    strlcpy(items[spot], add, size);
}

void _mallocandaddfolderbit(unsigned int *muhbits, int spot, bool value){
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
    _mallocandaddfolderbit(muhbits, 0, true);
    _mallocandaddfolderbit(muhbits, 1, true);

    if (f_opendir(&dir, path)) {
        gfx_printf("\nFailed to open %s", path);
        return 0;
    }
    else {
        while (!f_readdir(&dir, &fno) && fno.fname[0]){
            addchartoarray(fno.fname, items, i);
            _mallocandaddfolderbit(muhbits, i, fno.fattrib & AM_DIR);
            i++;
        }
    }
    f_closedir(&dir);
    return i;
}

int copy(const char *src, const char *dst){
    FIL in;
    FIL out;

    if (f_open(&in, src, FA_READ) != FR_OK){
        //something has gone wrong
        return 0;
    }
    if (f_open(&out, dst, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK){
        //something has gone wrong
        return 0;
    }

    int BUFFSIZ = 32768;
    u64 size = f_size(&in);
    void *buff = malloc(BUFFSIZ);
    int kbwritten = 0;
    gfx_printf("%d\n", size);
    while(size > BUFFSIZ){
        int res1, res2;
        res1 = f_read(&in, buff, BUFFSIZ, NULL);
        res2 = f_write(&out, buff, BUFFSIZ, NULL);

        kbwritten = kbwritten + BUFFSIZ;

        gfx_printf("Written %d\r", kbwritten);
        size = size - BUFFSIZ;
    }
    
    if(size != 0){
        f_read(&in, buff, size, NULL);
        f_write(&out, buff, size, NULL);
    }

    f_close(&in);
    f_close(&out);

    free(buff);

    return 1;
}

int copywithpath(const char *src, const char *dstpath, int mode){
    FILINFO fno;
    f_stat(src, &fno);
    char dst[PATHSIZE];
    strcpy(dst, dstpath);
    if (strcmp(dstpath, "sd:/") != 0) strcat(dst, "/");
    strcat(dst, fno.fname);
    int ret = -1;
    if (mode == 0) ret = copy(src, dst);
    if (mode == 1) f_rename(src, dst);
    return ret;
}