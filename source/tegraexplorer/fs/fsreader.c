#include "fsreader.h"
#include "../common/types.h"
#include "../common/common.h"
#include "fsutils.h"
#include "../../utils/types.h"
#include "../../libs/fatfs/ff.h"
#include "../gfx/gfxutils.h"
#include "../utils/utils.h"
#include "../../mem/heap.h"

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

void clearfileobjects(menu_entry **menu){
    if ((*menu) != NULL){
        for (int i = 0; (*menu)[i].name != NULL; i++){
            free((*menu)[i].name);
            (*menu)[i].name = NULL;
        }
        free((*menu));
        (*menu) = NULL;
    }
}

void createfileobjects(int size, menu_entry **menu){
    (*menu) = calloc (size + 1, sizeof(menu_entry));
    (*menu)[size].name = NULL;
}

void addobject(char* name, int spot, u8 attribs){
    u64 size = 0;
    int sizes = 0;
    fsreader_files[spot].property = 0;

    if (fsreader_files[spot].name != NULL){
        free(fsreader_files[spot].name);
        fsreader_files[spot].name = NULL;
    }

    utils_copystring(name, &(fsreader_files[spot].name));

    if (attribs & AM_DIR)
        fsreader_files[spot].property |= (ISDIR);
    else {
        /*
        size = fsutil_getfilesize(fsutil_getnextloc(currentpath, name));
        
        while (size > 1024){
            size /= 1024;
            sizes++;
        }

        if (sizes > 3)
            sizes = 0;

        fsreader_files[spot].property |= (1 << (4 + sizes));
        fsreader_files[spot].storage = size;
        */
       fsreader_files[spot].storage = 0;
       fsreader_files[spot].property = ISNULL;
    }

    if (attribs & AM_ARC)
        fsreader_files[spot].property |= (ISARC);
}

int fsreader_readfolder(const char *path){
    DIR dir;
    FILINFO fno;
    int folderamount, res;
    
    clearfileobjects(&fsreader_files);
    createfileobjects(fsutil_getfolderentryamount(path) + 3, &fsreader_files);

    for (folderamount = 0; folderamount < 3; folderamount++){
        utils_copystring(fs_menu_startdir[folderamount].name, &(fsreader_files[folderamount].name));
        fsreader_files[folderamount].storage = fs_menu_startdir[folderamount].storage;
        fsreader_files[folderamount].property = fs_menu_startdir[folderamount].property;
    }

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