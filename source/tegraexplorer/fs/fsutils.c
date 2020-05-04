#include <string.h>
#include "fsutils.h"
#include "../utils/utils.h"
#include "../common/types.h"
#include "../../utils/types.h"
#include "../../libs/fatfs/ff.h"
#include "../../mem/heap.h"
#include "../../utils/sprintf.h"
#include "../gfx/gfxutils.h"

char *fsutil_getnextloc(const char *current, const char *add){
    static char *ret;

    if (ret != NULL){
        free(ret);
        ret = NULL;
    }

    size_t size = strlen(current) + strlen(add) + 2;
    ret = (char*) malloc (size);

    if (current[strlen(current) - 1] == '/')
        sprintf(ret, "%s%s", current, add);
    else
        sprintf(ret, "%s/%s", current, add);

    return ret;
}

char *fsutil_getprevloc(char *current){
    static char *ret;
    char *temp;

    if (ret != NULL){
        free(ret);
        ret = NULL;
    }

    utils_copystring(current, &ret);
    temp = strrchr(ret, '/');

    if (*(temp - 1) == ':')
        temp++;

    *temp = '\0';

    return ret;
}

bool fsutil_checkfile(char* path){
    FRESULT fr;
    FILINFO fno;

    fr = f_stat(path, &fno);

    return !(fr & FR_NO_FILE);
}

u64 fsutil_getfilesize(char *path){
    FILINFO fno;
    f_stat(path, &fno);
    return fno.fsize;
}

int fsutil_getfolderentryamount(const char *path){
    DIR dir;
    FILINFO fno;
    int folderamount = 0, res;

    if ((res = f_opendir(&dir, path))){
        gfx_errDisplay("fsutil_getdircount", res, 0);
        return -1;
    }

    while (!f_readdir(&dir, &fno) && fno.fname[0]){
        folderamount++;
    }

    f_closedir(&dir);
    return folderamount;
}

char *fsutil_formatFileAttribs(char *path){
    FILINFO attribs;
    char *out;
    out = malloc(16);

    if (f_stat(path, &attribs))
        return NULL;

    sprintf(out, "Attribs: %c%c%c%c",
        (attribs.fattrib & AM_RDO) ? 'R' : '-',
        (attribs.fattrib & AM_SYS) ? 'S' : '-',
        (attribs.fattrib & AM_HID) ? 'H' : '-',
        (attribs.fattrib & AM_ARC) ? 'A' : '-');

    return out;
}