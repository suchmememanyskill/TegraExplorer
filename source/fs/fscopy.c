#include "fscopy.h"
#include <libs/fatfs/ff.h>
#include <utils/btn.h>
#include "../tegraexplorer/tconf.h"
#include "../gfx/gfx.h"
#include <mem/heap.h>
#include <string.h>
#include "../gfx/gfxutils.h"

ErrCode_t FileCopy(const char *locin, const char *locout, u8 options){
    FIL in, out;
    FILINFO in_info;
    u64 sizeRemaining, toCopy;
    u8 *buff;
    u32 x, y;
    int res = 0;

    gfx_con_getpos(&x, &y);

    if (!strcmp(locin, locout)){
        return newErrCode(TE_ERR_SAME_LOC);
    }

    if ((res = f_open(&in, locin, FA_READ | FA_OPEN_EXISTING))){
        return newErrCode(res);
    }

    if ((res = f_stat(locin, &in_info))){
        return newErrCode(res);
    }
       
    if ((res = f_open(&out, locout, FA_CREATE_ALWAYS | FA_WRITE))){
        return newErrCode(res);
    }

    if (options & COPY_MODE_PRINT){
        gfx_printf("[    ]");
        x += 16;
        gfx_con_setpos(x, y);
    }

    buff = malloc(TConf.FSBuffSize);
    sizeRemaining = f_size(&in);
    const u64 totalsize = sizeRemaining;

    while (sizeRemaining > 0){
        toCopy = MIN(sizeRemaining, TConf.FSBuffSize);

        if ((res = f_read(&in, buff, toCopy, NULL))){
            return newErrCode(res);
        }
            
        if ((res = f_write(&out, buff, toCopy, NULL))){
            return newErrCode(res);
        }

        sizeRemaining -= toCopy;

        if (options & COPY_MODE_PRINT){
            gfx_printf("%3d%%",  (u32)(((totalsize - sizeRemaining) * 100) / totalsize));
            gfx_con_setpos(x, y);
        }

        if (options & COPY_MODE_CANCEL && btn_read() & (BTN_VOL_DOWN | BTN_VOL_UP)){
            f_unlink(locout);
            break;
        }
    }

    f_close(&in);
    f_close(&out);
    free(buff);

    f_chmod(locout, in_info.fattrib, 0x3A);

    //f_stat(locin, &in_info); //somehow stops fatfs from being weird
    return newErrCode(0);
}