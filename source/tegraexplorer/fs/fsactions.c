#include <string.h>

#include "fsactions.h"
#include "../common/common.h"
#include "../../libs/fatfs/ff.h"
#include "../../gfx/gfx.h"
#include "../gfx/gfxutils.h"
#include "../utils/utils.h"
#include "../../mem/heap.h"
#include "../../utils/btn.h"
#include "fsutils.h"

int fsact_copy(const char *locin, const char *locout, u8 options){
    FIL in, out;
    FILINFO in_info;
    u64 sizeoffile, sizecopied = 0, totalsize;
    UINT temp1, temp2;
    u8 *buff;
    unsigned int x, y, i = 0;
    int res;

    gfx_con_getpos(&x, &y);

    if (!strcmp(locin, locout)){
        gfx_errDisplay("copy", ERR_SAME_LOC, 1);
        return 1;
    }

    if ((res = f_open(&in, locin, FA_READ | FA_OPEN_EXISTING))){
        gfx_errDisplay("copy", res, 2);
        return 1;
    }

    if (f_stat(locin, &in_info)){
        gfx_errDisplay("copy", res, 3);
        return 1;
    }
       
    if (f_open(&out, locout, FA_CREATE_ALWAYS | FA_WRITE)){
        gfx_errDisplay("copy", res, 4);
        return 1;
    }

    buff = malloc (BUFSIZE);
    sizeoffile = f_size(&in);
    totalsize = sizeoffile;

    while (sizeoffile > 0){
        if ((res = f_read(&in, buff, (sizeoffile > BUFSIZE) ? BUFSIZE : sizeoffile, &temp1))){
            gfx_errDisplay("copy", res, 5);
            return 1;
        }
            
        if ((res = f_write(&out, buff, (sizeoffile > BUFSIZE) ? BUFSIZE : sizeoffile, &temp2))){
            gfx_errDisplay("copy", res, 6);
            return 1;
        }
            
        if (temp1 != temp2){
            gfx_errDisplay("copy", ERR_DISK_WRITE_FAILED, 7);
            return 1;
        }

        sizeoffile -= temp1;
        sizecopied += temp1;

        if (options & COPY_MODE_PRINT && 10 > i++){
            gfx_printf("%k[%d%%]%k", COLOR_GREEN, ((sizecopied * 100) / totalsize) ,COLOR_WHITE);
            gfx_con_setpos(x, y);
            
            i = 0;

            if (options & COPY_MODE_CANCEL)
                if (btn_read() & BTN_VOL_DOWN){
                    f_unlink(locout);
                    break;
                }
        }
    }

    f_close(&in);
    f_close(&out);
    free(buff);

    if ((res = f_chmod(locout, in_info.fattrib, 0x3A))){
        gfx_errDisplay("copy", res, 8);
        return 1;
    }

    f_stat(locin, &in_info); //somehow stops fatfs from being weird
    return 0;
}

int fsact_del_recursive(char *path){
    DIR dir;
    FILINFO fno;
    int res;
    u32 x, y;
    char *localpath = NULL;

    gfx_con_getpos(&x, &y);
    utils_copystring(path, &localpath);

    if ((res = f_opendir(&dir, localpath))){
        gfx_errDisplay("del_recursive", res, 1);
        return 1;
    }

    while (!f_readdir(&dir, &fno) && fno.fname[0]){
        if (fno.fattrib & AM_DIR){
            fsact_del_recursive(fsutil_getnextloc(localpath, fno.fname));
        }
        else {
            SWAPCOLOR(COLOR_RED);
            gfx_printf("\r");
            gfx_printandclear(fno.fname, 37, 720);

            if ((res = f_unlink(fsutil_getnextloc(localpath, fno.fname)))){
                gfx_errDisplay("del_recursive", res, 2);
                return 1;
            }        
        }
    }

    f_closedir(&dir);

    if ((res = f_unlink(localpath))){
        gfx_errDisplay("del_recursive", res, 3);
        return 1;
    }

    free(localpath);
    return 0;
}

int fsact_copy_recursive(char *path, char *dstpath){
    DIR dir;
    FILINFO fno;
    int res;
    u32 x, y;
    char *startpath = NULL, *destpath = NULL, *destfoldername = NULL, *temp = NULL;

    gfx_con_getpos(&x, &y);

    utils_copystring(path, &startpath);
    utils_copystring(strrchr(path, '/') + 1, &destfoldername);
    utils_copystring(fsutil_getnextloc(dstpath, destfoldername), &destpath);

    if ((res = f_opendir(&dir, startpath))){
        gfx_errDisplay("copy_recursive", res, 1);
        return 1;
    }

    f_mkdir(destpath);

    while (!f_readdir(&dir, &fno) && fno.fname[0]){
        if (fno.fattrib & AM_DIR){
            fsact_copy_recursive(fsutil_getnextloc(startpath, fno.fname), destpath);
        }
        else {
            SWAPCOLOR(COLOR_GREEN);
            gfx_printf("\r");
            gfx_printandclear(fno.fname, 37, 720);

            utils_copystring(fsutil_getnextloc(startpath, fno.fname), &temp);

            if ((res = fsact_copy(temp, fsutil_getnextloc(destpath, fno.fname), COPY_MODE_PRINT))){
                gfx_errDisplay("copy_recursive", res, 2);
                return 1;
            }

            free(temp);
        }
    }
    
    f_closedir(&dir);
    
    if ((res = (f_stat(startpath, &fno)))){
        gfx_errDisplay("copy_recursive", res, 3);
        return 1;
    }

    if ((res = f_chmod(destpath, fno.fattrib, 0x3A))){
        gfx_errDisplay("copy_recursive", res, 4);
        return 1;
    }
    
    free(startpath);
    free(destpath);
    free(destfoldername);

    return 0;
}