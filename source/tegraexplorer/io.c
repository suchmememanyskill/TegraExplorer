#include <string.h>
#include "../mem/heap.h"
#include "gfx.h"
#include "fs.h"
#include "../utils/types.h"
#include "../libs/fatfs/ff.h"
#include "../utils/sprintf.h"
#include "../utils/btn.h"
#include "../gfx/gfx.h"
#include "../utils/util.h"
#include "io.h"

bool checkfile(char* path){
    FRESULT fr;
    FILINFO fno;

    fr = f_stat(path, &fno);

    return !(fr & FR_NO_FILE);
}

void viewbytes(char *path){
    FIL in;
    u8 print[2048];
    u32 size;
    QWORD offset = 0;
    int res;

    clearscreen();
    res = f_open(&in, path, FA_READ | FA_OPEN_EXISTING);
    if (res != FR_OK){
        message(COLOR_RED, "File Opening Failed\nErrcode %d", res);
        return;
    }

    msleep(200);

    while (1){
        f_lseek(&in, offset * 16);

        res = f_read(&in, &print, 2048 * sizeof(u8), &size);
        if (res != FR_OK){
            message(COLOR_RED, "Reading Failed!\nErrcode %d", res);
            return;
        }

        printbytes(print, size, offset * 16);
        res = btn_read();

        if (!res)
            res = btn_wait();

        if (res & BTN_VOL_DOWN && 2048 * sizeof(u8) == size)
            offset++;
        if (res & BTN_VOL_UP && offset > 0)
            offset--;
        if (res & BTN_POWER)
            break;
    }
    f_close(&in);
}

int copy(const char *locin, const char *locout, bool print, bool canCancel){
    FIL in, out;
    FILINFO in_info;
    u64 sizeoffile, sizecopied = 0, totalsize;
    UINT temp1, temp2;
    u8 *buff;
    unsigned int x, y, i = 0;
    int res;

    gfx_con_getpos(&x, &y);

    if (!strcmp(locin, locout)){
        return 21;
    }

    if (f_open(&in, locin, FA_READ | FA_OPEN_EXISTING)){
        return 22;
    }

    if (f_stat(locin, &in_info))
        return 22;

    if (f_open(&out, locout, FA_CREATE_ALWAYS | FA_WRITE)){
        return 23;
    }

    buff = malloc (BUFSIZE);
    sizeoffile = f_size(&in);
    totalsize = sizeoffile;

    while (sizeoffile > 0){
        if ((res = f_read(&in, buff, (sizeoffile > BUFSIZE) ? BUFSIZE : sizeoffile, &temp1)))
            return res;
        if ((res = f_write(&out, buff, (sizeoffile > BUFSIZE) ? BUFSIZE : sizeoffile, &temp2)))
            return res;

        if (temp1 != temp2)
            return 24;

        sizeoffile -= temp1;
        sizecopied += temp1;

        if (print && 10 > i++){
            gfx_printf("%k[%d%%]%k", COLOR_GREEN, ((sizecopied * 100) / totalsize) ,COLOR_WHITE);
            gfx_con_setpos(x, y);
            
            i = 0;

            if (canCancel)
                if (btn_read() & BTN_VOL_DOWN){
                    f_unlink(locout);
                    break;
                }
        }
    }

    f_close(&in);
    f_close(&out);
    free(buff);

    if ((res = f_chmod(locout, in_info.fattrib, 0x3A)))
        return res;

    return 0;
}

u64 getfilesize(char *path){
    FILINFO fno;
    f_stat(path, &fno);
    return fno.fsize;
}

int getfolderentryamount(const char *path){
    DIR dir;
    FILINFO fno;
    int folderamount = 0;

    if ((f_opendir(&dir, path))){
        return -1;
    }

    while (!f_readdir(&dir, &fno) && fno.fname[0]){
        folderamount++;
    }

    f_closedir(&dir);
    return folderamount;
}

void makestring(char *in, char **out){
    *out = (char *) malloc (strlen(in) + 1);
    strcpy(*out, in);
}

int del_recursive(char *path){
    DIR dir;
    FILINFO fno;
    int res;
    char *localpath = NULL;
    makestring(path, &localpath);

    if ((res = f_opendir(&dir, localpath))){
        message(COLOR_RED, "Error during f_opendir: %d", res);
        return -1;
    }

    while (!f_readdir(&dir, &fno) && fno.fname[0]){
        if (fno.fattrib & AM_DIR)
            del_recursive(getnextloc(localpath, fno.fname));

        else {
            gfx_box(0, 47, 719, 63, COLOR_DEFAULT);
            SWAPCOLOR(COLOR_RED);
            gfx_printf("\r");
            gfx_print_length(37, fno.fname);
            gfx_printf(" ");

            if ((res = f_unlink(getnextloc(localpath, fno.fname))))
                return res;
        }
    }

    f_closedir(&dir);

    if ((res = f_unlink(localpath))){
        return res;
    }

    free(localpath);

    return 0;
}

int copy_recursive(char *path, char *dstpath){
    DIR dir;
    FILINFO fno;
    int res;
    char *startpath = NULL, *destpath = NULL, *destfoldername = NULL, *temp = NULL;

    makestring(path, &startpath);
    makestring(strrchr(path, '/') + 1, &destfoldername);

    destpath = (char*) malloc (strlen(dstpath) + strlen(destfoldername) + 2);
    sprintf(destpath, (dstpath[strlen(dstpath) - 1] == '/') ? "%s%s" : "%s/%s", dstpath, destfoldername);
    

    if ((res = f_opendir(&dir, startpath))){
        message(COLOR_RED, "Error during f_opendir: %d", res);
        return 21;
    }

    f_mkdir(destpath);

    if (f_stat(startpath, &fno))
        return 22;

    while (!f_readdir(&dir, &fno) && fno.fname[0]){
        if (fno.fattrib & AM_DIR){
            copy_recursive(getnextloc(startpath, fno.fname), destpath);
        }
        else {
            gfx_box(0, 47, 719, 63, COLOR_DEFAULT);
            SWAPCOLOR(COLOR_GREEN);
            gfx_printf("\r");
            gfx_print_length(37, fno.fname);
            gfx_printf(" ");
            makestring(getnextloc(startpath, fno.fname), &temp);

            if ((res = copy(temp, getnextloc(destpath, fno.fname), true, false))){
                return res;
            }

            free(temp);
        }
    }
    
    f_closedir(&dir);
    free(startpath);
    free(destpath);
    free(destfoldername);

    if ((res = f_chmod(destpath, fno.fattrib, 0x3A)))
        return res;

    return 0;
}