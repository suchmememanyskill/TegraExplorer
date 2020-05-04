#include "fsreader.h"
#include "../common/types.h"
#include "../common/common.h"
#include "fsutils.h"
#include "../../utils/types.h"
#include "../../libs/fatfs/ff.h"
#include "../gfx/gfxutils.h"
#include "../utils/utils.h"
#include "../../mem/heap.h"
#include "../utils/menuUtils.h"

menu_entry *fsreader_files = NULL;
char *currentpath = NULL;
char *clipboard = NULL;
u8 clipboardhelper = 0;

void fsreader_writecurpath(const char *in){
    if (currentpath != NULL)
        free(currentpath);

    utils_copystring(in, &currentpath);
}

void fsreader_writeclipboard(const char *in, u8 args){
    if (clipboard != NULL)
        free(clipboard);

    clipboardhelper = args;

    utils_copystring(in, &clipboard);
}


void addobject(char* name, int spot, u8 attribs){
    u8 property = 0;

    if (attribs & AM_DIR)
        property |= ISDIR;
    else
        property |= ISNULL;

    mu_copySingle(name, 0, property, &fsreader_files[spot]);
}

int fsreader_readfolder(const char *path){
    DIR dir;
    FILINFO fno;
    int folderamount, res;
    
    mu_clearObjects(&fsreader_files);
    mu_createObjects(fsutil_getfolderentryamount(path) + 3, &fsreader_files);



    for (folderamount = 0; folderamount < 3; folderamount++)
        mu_copySingle(fs_menu_startdir[folderamount].name, fs_menu_startdir[folderamount].storage, fs_menu_startdir[folderamount].property, &fsreader_files[folderamount]);

    if ((res = f_opendir(&dir, path))){
        gfx_errDisplay("readfolder", res, 0);
        return -1;
    }

    while (!f_readdir(&dir, &fno) && fno.fname[0]){
        addobject(fno.fname, folderamount++, fno.fattrib);
    }

    f_closedir(&dir);
    return folderamount;
}