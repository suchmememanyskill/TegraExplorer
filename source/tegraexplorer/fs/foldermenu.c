#include <string.h>
#include "entrymenu.h"
#include "../common/common.h"
#include "../../libs/fatfs/ff.h"
#include "../../mem/heap.h"
#include "../gfx/menu.h"
#include "fsreader.h"
#include "../gfx/gfxutils.h"
#include "fsactions.h"
#include "fsutils.h"
#include "../../utils/sprintf.h"
#include "../utils/utils.h"
#include "../../hid/hid.h"
#include "../utils/menuUtils.h"

extern char *currentpath;
extern char *clipboard;
extern u8 clipboardhelper;

void copyfolder(char *in, char *out){
    int res;

    res = strlen(in);
    if ((*(in + res - 1) == '/')){
        gfx_errDisplay("copyfolder", ERR_FOLDER_ROOT, 1);
    }
    else if (strstr(out, in) != NULL){
        gfx_errDisplay("copyfolder", ERR_DEST_PART_OF_SRC, 2);
    }
    else if (!strcmp(in, out)){
        gfx_errDisplay("copyfolder", ERR_SAME_LOC, 3);
    }
    else {
        gfx_clearscreen();
        gfx_printf("\nCopying folder, please wait\n");
        fsact_copy_recursive(in, out);
        fsreader_readfolder(currentpath);
    }

    clipboardhelper = 0;
}

int foldermenu(){
    int res, hidConn;
    char *name;

    hidConn = hidConnected();

    res = strlen(currentpath);

    fs_menu_folder[3].isHide = (*(currentpath + res - 1) == '/');
    fs_menu_folder[4].isHide = (*(currentpath + res - 1) == '/');
    fs_menu_folder[5].isHide = (*(currentpath + res - 1) == '/' || !hidConn);
    fs_menu_folder[6].isHide = !hidConn;

    /*

    SETBIT(fs_menu_folder[3].property, ISHIDE, (*(currentpath + res - 1) == '/'));
    SETBIT(fs_menu_folder[4].property, ISHIDE, (*(currentpath + res - 1) == '/'));
    SETBIT(fs_menu_folder[5].property, ISHIDE, (*(currentpath + res - 1) == '/') || !hidConn);
    SETBIT(fs_menu_folder[6].property, ISHIDE, !hidConn);
    
    if (f_stat(currentpath, &attribs))
        SETBIT(fs_menu_folder[0].property, ISHIDE, 1);
    else {
        SETBIT(fs_menu_folder[0].property, ISHIDE, 0);
        sprintf(fs_menu_folder[0].name, "Attribs: %c%c%c%c",
        (attribs.fattrib & AM_RDO) ? 'R' : '-',
        (attribs.fattrib & AM_SYS) ? 'S' : '-',
        (attribs.fattrib & AM_HID) ? 'H' : '-',
        (attribs.fattrib & AM_ARC) ? 'A' : '-');
    }
    */

    if ((name = fsutil_formatFileAttribs(currentpath)) == NULL){
        fs_menu_folder[0].isHide = 1;
    }
    else {
        fs_menu_folder[0].isHide = 0;
        mu_copySingle(name, fs_menu_folder[0].storage, fs_menu_folder[0].property, &fs_menu_folder[0]);
    }



    res = menu_make(fs_menu_folder, 7, currentpath);

    switch (res){
        case DIR_EXITFOLDER:
        case -1:
            return -1;
        case DIR_COPYFOLDER:
            fsreader_writeclipboard(currentpath, OPERATIONCOPY | ISDIR);
            break;
        case DIR_DELETEFOLDER:
            gfx_clearscreen();
            if (gfx_defaultWaitMenu("Do you want to delete this folder?\nThe entire folder, with all subcontents will be deleted!", 2)){
                gfx_clearscreen();
                gfx_printf("\nDeleting folder, please wait...\n");

                fsact_del_recursive(currentpath);

                fsreader_writecurpath(fsutil_getprevloc(currentpath));
                fsreader_readfolder(currentpath);
            }
            break;
        case DIR_RENAME:;
            char *prevLoc, *dirName;

            dirName = strrchr(currentpath, '/') + 1;

            gfx_clearscreen();
            gfx_printf("Renaming %s...\n\n", dirName); 
            name = utils_InputText(dirName, 39);
            if (name == NULL)
                break;
            
            utils_copystring(fsutil_getprevloc(currentpath), &prevLoc);
            res = f_rename(currentpath, fsutil_getnextloc(prevLoc, name));

            free(prevLoc);
            free(name);

            if (res){
                gfx_errDisplay("folderMenu", res, 1);
                break;
            }

            fsreader_writecurpath(fsutil_getprevloc(currentpath));
            fsreader_readfolder(currentpath);

            break;
        case DIR_CREATE:;
            gfx_clearscreen();
            gfx_printf("Give a name for your new folder\n\n");
            name = utils_InputText("New Folder", 39);
            if (name == NULL)
                break;

            res = f_mkdir(fsutil_getnextloc(currentpath, name));
            free(name);

            if (res){
                gfx_errDisplay("folderMenu", res, 1);
                break;
            }

            fsreader_readfolder(currentpath);
            break;
    }

    return 0;
}